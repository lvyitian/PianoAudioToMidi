#include "stdafx.h"
#include "AudioLoader.h"
#include "MonoResampler.h"
#include "Frame.h"
#include "Packet.h"

using namespace std;

struct FFmpegData
{
	AVFormatContext* formatContext = nullptr;

	AVCodecParameters* codecParams = nullptr;
	AVCodec* codec = nullptr;
	AVStream* audioStream = nullptr;

	AVCodecContext* codecContext = nullptr;
	AVFrame* frame = nullptr;
	AVPacket* packet = nullptr;

	vector<uint8_t> rawData;
};

AudioLoader::AudioLoader(const char* fileName) : data_(make_unique<FFmpegData>())
{
	if (avformat_open_input(&data_->formatContext, fileName, nullptr, nullptr))
		throw FFmpegError(("Could not open file: " + string(fileName)).c_str());
	if (avformat_find_stream_info(data_->formatContext, nullptr) < 0)
		throw FFmpegError("Could not find the stream info");

	FindAudioStream();
}
AudioLoader::~AudioLoader()
{
	avformat_close_input(&data_->formatContext);
	avformat_free_context(data_->formatContext);
	if (data_->packet) av_packet_free(&data_->packet);
	if (data_->frame) av_frame_free(&data_->frame);
	if (data_->codecContext)
	{
		avcodec_close(data_->codecContext);
		avcodec_free_context(&data_->codecContext);
	}
}

const char* AudioLoader::GetFormatName() const { return data_->formatContext->iformat->long_name; }
const char* AudioLoader::GetCodecName() const { return data_->codec->long_name; }
int64_t AudioLoader::GetBitRate() const { return data_->codecParams->bit_rate; }

const vector<uint8_t>& AudioLoader::GetRawData() const
{
	assert("Did you call Decode()?" && !data_->rawData.empty());
	return data_->rawData;
}
size_t AudioLoader::GetNumSeconds() const
{
	assert("Did you call Decode()?" && !data_->rawData.empty());
	auto bytesPerSample(av_get_bytes_per_sample(data_->codecContext->sample_fmt));
	if (bytesPerSample == 0) throw FFmpegError("Could not determine number of bytes per sample");
	return data_->rawData.size() / bytesPerSample / data_->codecContext->sample_rate;
}

#pragma warning(push)
#pragma warning(disable: 4711) // Automatic inline expansion
#pragma warning(disable: 5045) // Compiler will insert Spectre mitigation for memory load
void AudioLoader::FindAudioStream() const
{
	for (size_t i(0); i < data_->formatContext->nb_streams; ++i)
		if (data_->formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			data_->codecParams = data_->formatContext->streams[i]->codecpar;
			data_->codec = avcodec_find_decoder(data_->codecParams->codec_id);
			if (!data_->codec) throw FFmpegError("Unsupported codec :(");
			data_->audioStream = data_->formatContext->streams[i];
			break;
		}
	if (!data_->audioStream) throw FFmpegError("Could not find audio stream");
}
#pragma warning(pop)

int AudioLoader::DecodePacket() const
{
	char errStr[AV_ERROR_MAX_STRING_SIZE];
	auto response(avcodec_send_packet(data_->codecContext, data_->packet));
	if (response < 0) throw FFmpegError(("Could not send a packet to the decoder:\n"
		+ string(av_make_error_string(errStr, sizeof errStr / sizeof *errStr, response))).c_str());
	while (response >= 0)
	{
		Frame frame(data_->frame);
		response = frame.Receive(data_->codecContext);
		if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) break;
		else if (response < 0) throw FFmpegError(("Could not receive a frame from the decoder:\n"
			+ string(av_make_error_string(errStr, sizeof errStr / sizeof *errStr, response))).c_str());

		// We now have a fully decoded audio frame
		if (data_->codecContext->channels == 2) assert (data_->frame->linesize[1] == 0);
		else assert(data_->codecContext->channels == 1);
		// There could be discarded samples for MP3, so use linesize instead of nb_samples * nBlockAlign
		const auto iter(data_->rawData.insert(data_->rawData.cend(),
			data_->frame->extended_data[0], data_->frame->extended_data[0] + data_->frame->linesize[0]));
	}
	return 0;
}
void AudioLoader::Decode() const
{
	data_->codecContext = avcodec_alloc_context3(data_->codec);
	if (!data_->codecContext) throw FFmpegError("Could not allocate memory for codec context");
	if (avcodec_parameters_to_context(data_->codecContext, data_->codecParams) < 0)
		throw FFmpegError("Could not convert codec params to codec context");
	if (avcodec_open2(data_->codecContext, data_->codec, nullptr) < 0)
		throw FFmpegError("Could not open codec");

	data_->frame = av_frame_alloc();
	if (!data_->frame) throw FFmpegError("Could not allocate memory for frame");
	data_->packet = av_packet_alloc();
	if (!data_->packet) throw FFmpegError("Could not allocate memory for packet");

	while (true)
	{
		Packet packet(data_->packet);
		if (packet.Read(data_->formatContext) or (
			data_->packet->stream_index == data_->audioStream->index and DecodePacket() < 0)) break;
	}
	// Some codecs buffer up frames during decoding.
	// If the flag below is set, possibly buffered up frames need to be flushed
	// Decode all the remaining frames in the buffer, until the end is reached
	if (data_->codecContext->codec->capabilities & AV_CODEC_CAP_DELAY) while (DecodePacket() >= 0);
}

void AudioLoader::MonoResample(int rate, const AVSampleFormat format) const
{
	assert("Did you call Decode()?" && !data_->rawData.empty());

	if (rate == 0) rate = data_->codecContext->sample_rate;
	MonoResampler resampler;
	const auto result(resampler.Resample(data_->rawData.data(), data_->rawData.size(),
		data_->codecContext->channels, data_->codecContext->sample_rate, rate,
		data_->codecContext->sample_fmt, format));
	data_->rawData.assign(result.first, result.first + result.second);

	data_->codecContext->channels = 1;
	data_->codecContext->sample_rate = rate;
	data_->codecContext->sample_fmt = format;
}