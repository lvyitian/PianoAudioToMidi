#include "stdafx.h"
#include "AudioLoader.h"
//#include "DirectSound.h"
//#include "ShortTimeFourier.h"
#include "CqBasis.h"

int main()
{
	using namespace std;
	namespace p = boost::python;
	namespace np = p::numpy;

	vector<float> rawSong;
	try
	{
		const AudioLoader song("C:/Users/Boris/Documents/Visual Studio 2017/Projects/Piano 2 Midi/Test songs/"
//			"chpn_op7_1MINp_align.wav"
//			"MAPS_MUS-chpn-p7_SptkBGCl.wav"
			"Eric Clapton - Wonderful Tonight.wma"
//			"Chopin_op10_no3_p05_aligned.mp3"
		);
		cout << "Format:\t\t" << song.GetFormatName() << endl
			<< "Audio Codec:\t" << song.GetCodecName() << endl
			<< "Bit_rate:\t" << song.GetBitRate() << endl;

		song.Decode();
		cout << "Duration:\t" << song.GetNumSeconds() / 60 << " min : "
			<< song.GetNumSeconds() % 60 << " sec" << endl;

		song.MonoResample();
		const auto iter(rawSong.insert(rawSong.cend(), reinterpret_cast<const float*>(song.GetRawData().data()),
			reinterpret_cast<const float*>(song.GetRawData().data() + song.GetRawData().size())));

//		DirectSound sound;
//		song.MonoResample(0, AV_SAMPLE_FMT_S16);
//		WAVEFORMATEX wavHeader{ WAVE_FORMAT_PCM, 1, 22'050, 44'100, 2, 16 };
//		const auto wavStart(song.GetRawData().data());
//		const auto wavSize(song.GetRawData().size());
//		sound.Play(&wavHeader, wavStart, wavSize);
//		system("Pause");
	}
	catch (const FFmpegError& e) { cout << e.what() << endl; }
//	catch (const SoundError& e) { cout << e.what() << endl; }

//	ShortTimeFourier stft(rawSong);

	const CqBasis qBasis(22'050, 27.5);

	try
	{
		Py_SetPath(L"C:/Anaconda3/Lib");
		Py_Initialize();

		const auto pathAppend(p::import("sys").attr("path").attr("append"));
		auto pythonLib(pathAppend("C:/Anaconda3/Lib/site-packages"));
		pythonLib = pathAppend("C:/Anaconda3/DLLs");
		np::initialize();

		auto main_namespace = p::import("__main__").attr("__dict__");

		class Song
		{
		public:
			Song() : data(np::empty(p::tuple(1), np::dtype::get_builtin<float>())) {}
			explicit Song(const np::ndarray& newData) : data(newData) {}
			np::ndarray data;
		};
		main_namespace["CppStructSong"] = p::class_<Song>("CppStructSong").add_property(
			"data", p::make_getter(&Song::data));
		Song pythonSong(np::from_data(rawSong.data(), np::dtype::get_builtin<float>(),
			p::make_tuple(rawSong.size()), p::make_tuple(sizeof rawSong.front()), p::object()));
		main_namespace["cppSong"] = p::ptr(&pythonSong);
		
		// 1 Load the song:
		const auto pyScript1(p::exec(R"(
print(cppSong.data.shape, cppSong.data.min(), cppSong.data.mean(), cppSong.data.max())

import librosa as lbr

#songName = '../../../Test songs/chpn_op7_1MINp_align.wav'
songName = '../../../Test songs/Eric Clapton - Wonderful Tonight.wma'
#songName = '../../../Test songs/Chopin_op10_no3_p05_aligned.mp3'

harmMargin, nBins, nFrames = 4, 5, 5

#song = lbr.effects.trim(lbr.load(songName)[0])[0]
song = lbr.load(songName)[0]
print(song.shape, song.min(), song.mean(), song.max())
songLen = int(lbr.get_duration(song))
print('Song duration\t{} min : {} sec'.format(songLen // 60, songLen % 60))

from matplotlib import pyplot as plt

plt.figure(figsize=(16, 8))

ax1 = plt.subplot(2, 1, 1)
plt.title('Cpp WaveForm')
plt.plot(range(200_000), cppSong.data[1_000_000:1_200_000])

ax2 = plt.subplot(2, 1, 2)
plt.title('Python WaveForm')
plt.plot(range(200_000), song[1_000_000:1_200_000])

plt.show()
)", main_namespace));

		class CQbasis
		{
		public:
			CQbasis() : data(np::empty(p::make_tuple(1, 1), np::dtype::get_builtin<complex<float>>())) {}
			explicit CQbasis(const np::ndarray& newData) : data(newData) {}
			np::ndarray data;
		};
		main_namespace["CppStructCQbasis"] = p::class_<CQbasis>("CppStructCQbasis").add_property(
			"data", p::make_getter(&CQbasis::data));

		vector<complex<float>> qBasisData(qBasis.GetCqFilters().size()
			* qBasis.GetCqFilters().front().size());
		for (size_t i(0); i < qBasis.GetCqFilters().size(); ++i) const auto unusedIter(copy(
			qBasis.GetCqFilters().at(i).cbegin(), qBasis.GetCqFilters().at(i).cend(),
			qBasisData.begin() + i * qBasis.GetCqFilters().front().size()));

		CQbasis pythonCQbasis(np::from_data(qBasisData.data(), np::dtype::get_builtin<complex<float>>(),
			p::make_tuple(qBasis.GetCqFilters().size(), qBasis.GetCqFilters().front().size()),
			p::make_tuple(qBasis.GetCqFilters().front().size() * sizeof qBasisData.front(),
				sizeof qBasis.GetCqFilters().front().front()), p::object()));
		main_namespace["cppCQbasis"] = p::ptr(&pythonCQbasis);

		const auto pyScript1_2(p::exec(R"(
from librosa.display import specshow
import numpy as np

#Plot one octave of filters in time and frequency
basis = cppCQbasis.data

plt.figure(figsize=(16, 4))

plt.subplot(1, 2, 1)
for i, f in enumerate(basis[:12]):
    f_scale = lbr.util.normalize(f) / 2
    plt.plot(i + f_scale.real)
    plt.plot(i + f_scale.imag, linestyle=':')

notes = lbr.midi_to_note(np.arange(24, 24 + len(basis)))

plt.yticks(np.arange(12), notes[:12])
plt.ylabel('CQ filters')
plt.title('CQ filters (one octave, time domain)')
plt.xlabel('Time (samples at 22050 Hz)')
plt.legend(['Real', 'Imaginary'])

plt.subplot(1, 2, 2)
F = np.abs(np.fft.fftn(basis, axes=[-1]))
# Keep only the positive frequencies
F = F[:, :(1 + F.shape[1] // 2)]

specshow(F, x_axis='linear')
plt.yticks(np.arange(len(notes))[::12], notes[::12])
plt.ylabel('CQ filters')
plt.title('CQ filter magnitudes (frequency domain)')

plt.show()
)", main_namespace));

		system("Pause");
		return 0;

		/*
		class STFT
		{
		public:
			STFT() : data(np::empty(p::make_tuple(1, 1), np::dtype::get_builtin<complex<float>>())) {}
			explicit STFT(const np::ndarray& newData) : data(newData) {}
			np::ndarray data;
		};
		main_namespace["CppStructSTFT"] = p::class_<STFT>("CppStructSTFT").add_property(
			"data", p::make_getter(&STFT::data));

		vector<complex<float>> stftData(stft.GetSTFT().size() * stft.GetSTFT().front().size());
		for (size_t i(0); i < stft.GetSTFT().size(); ++i) const auto unusedIter(copy(
			stft.GetSTFT().at(i).cbegin(), stft.GetSTFT().at(i).cend(),
			stftData.begin() + i * stft.GetSTFT().front().size()));

		STFT pythonSTFT(np::from_data(stftData.data(), np::dtype::get_builtin<complex<float>>(),
			p::make_tuple(stft.GetSTFT().size(), stft.GetSTFT().front().size()),
			p::make_tuple(stft.GetSTFT().front().size() * sizeof stftData.front(),
				sizeof stft.GetSTFT().front().front()), p::object()));
		main_namespace["cppSTFT"] = p::ptr(&pythonSTFT);

		const auto pyScript1_2(p::exec(R"(
from librosa.display import specshow

print(cppSTFT.data.shape, cppSTFT.data.min(), cppSTFT.data.mean(), cppSTFT.data.max())
stft = lbr.stft(song)
print(stft.shape, stft.min(), stft.mean(), stft.max())

plt.figure(figsize=(16, 8))

ax1 = plt.subplot(2, 1, 1)
plt.title('Cpp Power spectrogram')
specshow(lbr.amplitude_to_db(lbr.magphase(cppSTFT.data.T)[0]), y_axis='log', x_axis='time')

ax2 = plt.subplot(2, 1, 2)
plt.title('Python Power spectrogram')
specshow(lbr.amplitude_to_db(lbr.magphase(stft)[0]), y_axis='log', x_axis='time')

plt.show()
)", main_namespace));
		*/

		// 2 CQT-transform:
		const auto pyScript2(p::exec(R"(
import numpy as np

songHarm = lbr.effects.harmonic(song, margin=harmMargin)
cqts = lbr.util.normalize(lbr.amplitude_to_db(lbr.magphase(lbr.cqt(
    songHarm, fmin=lbr.note_to_hz('A0'), n_bins=88*nBins, bins_per_octave=12*nBins))[0], ref=np.min))
print(cqts.shape[1], 'frames,', end='\t')
#assert cqts.min() > 0 and cqts.max() == 1 and len(np.hstack([(c == 1).nonzero()[0] for c in cqts.T])) == cqts.shape[1]
print('normalized cqts in range [{:.2e} - {:.3f} - {:.3f}]'.format(cqts.min(), cqts.mean(), cqts.max()))

# Not using row-wise batch norm of each cqt-bin, even though:
maxBins = cqts.max(1)
assert maxBins.shape == (440,)
print('All maximums should be ones, but there are only {:.1%} ones and min of max is {:.3f}'.format(
    maxBins[maxBins == 1].sum() / len(maxBins), maxBins.min()))

print('And now the same for Cpp data:')
songHarmCpp = lbr.effects.harmonic(cppSong.data, margin=harmMargin)
cqtsCpp = lbr.util.normalize(lbr.amplitude_to_db(lbr.magphase(lbr.cqt(
    songHarmCpp, fmin=lbr.note_to_hz('A0'), n_bins=88*nBins, bins_per_octave=12*nBins))[0], ref=np.min))
print(cqtsCpp.shape[1], 'frames,', end='\t')
print(len(np.hstack([(c == 1).nonzero()[0] for c in cqtsCpp.T])), 'ones')
assert cqtsCpp.min() > 0 and cqtsCpp.max() == 1 # and len(np.hstack([(c == 1).nonzero()[0] for c in cqtsCpp.T])) == cqts.shape[1]
print('normalized cqts in range [{:.2e} - {:.3f} - {:.3f}]'.format(cqtsCpp.min(), cqtsCpp.mean(), cqtsCpp.max()))

# Not using row-wise batch norm of each cqt-bin, even though:
maxBins = cqtsCpp.max(1)
assert maxBins.shape == (440,)
print('All maximums should be ones, but there are only {:.1%} ones and min of max is {:.3f}'.format(
    maxBins[maxBins == 1].sum() / len(maxBins), maxBins.min()))
)", main_namespace));

		// 3 Note onsets:
		const auto pyScript3(p::exec(R"(
plt.figure(figsize=(16, 8))
ax1 = plt.subplot(2, 1, 1)
plt.title('Cpp Power spectrogram')
specshow(cqtsCpp, x_axis='time', y_axis='cqt_note', fmin=lbr.note_to_hz('A0'), bins_per_octave=12*nBins)

ax2 = plt.subplot(2, 1, 2)
plt.title('Python Power spectrogram')
specshow(cqts, x_axis='time', y_axis='cqt_note', fmin=lbr.note_to_hz('A0'), bins_per_octave=12*nBins)

#o_env, frames = lbr.onset.onset_strength(song), lbr.onset.onset_detect(song)
'''
times = lbr.frames_to_time(range(len(o_env)))
plt.vlines(times[frames], lbr.note_to_hz('A0'), lbr.note_to_hz('C8'), color='w', alpha=.8)

ax3 = plt.subplot(3, 1, 3)
plt.plot(times, o_env, label='Onset strength')
plt.vlines(times[frames], 0, o_env.max(), color='r', label='Onsets')
plt.legend()
plt.ylim(0, o_env.max())
'''
for a in [ax1, ax2]: a.set_xlim(0, 10)

plt.show()
)", main_namespace));

		system("Pause");
		return 0;

		// 4 Key Signature Estimation, Option 1
		// From Kumhansl and Schmuckler as reported here: http://rnhart.net/articles/key-finding/
		const auto pyScript4(p::exec(R"(
chroma = lbr.feature.chroma_cqt(songHarm)[:, frames].sum(1)
major = [np.corrcoef(chroma, np.roll([6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88], i))[0, 1] for i in range(12)]
minor = [np.corrcoef(chroma, np.roll([6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17], i))[0, 1] for i in range(12)]

keySignature = (['C', 'C#', 'D', 'Eb', 'E', 'F', 'F#', 'G', 'Ab', 'A', 'Bb', 'B'][
    major.index(max(major)) if max(major) > max(minor) else minor.index(max(minor)) - 3]
                + ('m' if max(major) < max(minor) else ''))
print(keySignature)
)", main_namespace));

		// 5 Pre-Processing of Spectrogram Data
		const auto pyScript5(p::exec(R"(
cqts = np.vstack([np.zeros((nFrames // 2, cqts.shape[0]), cqts.dtype), cqts.T,
                  np.zeros((nFrames // 2, cqts.shape[0]), cqts.dtype)])
cqts = np.array([cqts[range(f, f + nFrames)] for f in range(len(cqts) - nFrames + 1)])
print('Cqts grouped.')
)", main_namespace));

		// 6 Load CNN-Model and Predict Probabilities
		const auto pyScript6(p::exec(R"(
from keras.models import load_model
from tensorflow import device

with device('/cpu:0'):
	model = load_model('../../../Model Train71 Val65 Frame59.h5') # HDF5 file
	yProb = model.predict(cqts, verbose=1)
)", main_namespace));

		// 7 Keep Only Notes with Probabilities > some threshold
		// There are false positives in high octaves, probably because
		// we provide less cqt harmonics for them in CNN.
		// If it is too annoing, you can use higher threshold for higher octaves.
		// Can't think of any better solution for now :(
		const auto pyScript7(p::exec(R"(
from collections import Counter
from sklearn.preprocessing import binarize

#yPredAll = np.hstack([binarize(yProb[:, :16], .1), binarize(yProb[:, 16:28], .3), binarize(yProb[:, 28:40], .3),
#                      binarize(yProb[:, 40:52], .3), binarize(yProb[:, 52:64], .4), binarize(yProb[:, 64:], .9)])
yPredAll = binarize(yProb, .5)
yPred = yPredAll.sum(0)
if len(frames) > 1:
    yPred = yPredAll[:(frames[0] + frames[1]) // 2].sum(0)
    for i, fr in enumerate(frames[1:-1]):
        yPred = np.vstack([yPred, yPredAll[(frames[i] + fr) // 2 : (fr + frames[i + 2]) // 2].sum(0)])
    yPred = np.vstack([yPred, yPredAll[(frames[-2] + frames[-1]) // 2 :].sum(0)])

notes = Counter()
for row in yPred: notes.update(Counter(np.argwhere(row).ravel() % 12))
gamma = [n for _, n in sorted([(count, ['A', 'Bb', 'B', 'C', 'C#', 'D', 'Eb', 'E', 'F', 'F#', 'G', 'Ab'][i])
                for i, count in notes.items()], reverse=True)[:7]]
blacks = sorted(n for n in gamma if len(n) > 1)
print(blacks, gamma)
)", main_namespace));

		// 8 Key Signature Estimation, Option 2
		const auto pyScript8(p::exec(R"(
MajorMinor = lambda mj, mn: mj if gamma.index(mj) < gamma.index(mn) else mn + 'm'

if len(blacks) == 0: keySignature = MajorMinor('C', 'A')

elif len(blacks) == 1:
    if blacks[0] == 'F#':
        assert 'F' not in gamma
        keySignature = MajorMinor('G', 'E')
    elif blacks[0] == 'Bb':
        assert 'B' not in gamma
        keySignature = MajorMinor('F', 'D')
    else: assert False

elif len(blacks) == 2:
    if blacks == ['C#', 'F#']:
        assert 'C' not in gamma and 'F' not in gamma
        keySignature = MajorMinor('D', 'B')
    elif blacks == ['Bb', 'Eb']:
        assert 'B' not in gamma and 'E' not in gamma
        keySignature = MajorMinor('Bb', 'G')
    else: assert False

elif len(blacks) == 3:
    if blacks == ['Ab', 'C#', 'F#']:
        assert 'C' not in gamma and 'F' not in gamma and 'G' not in gamma
        keySignature = MajorMinor('A', 'F#')
    elif blacks == ['Ab', 'Bb', 'Eb']:
        assert 'B' not in gamma and 'E' not in gamma and 'A' not in gamma
        keySignature = MajorMinor('Eb', 'C')
    else: assert False

elif len(blacks) == 4:
    if blacks == ['Ab', 'C#', 'Eb', 'F#']:
        assert 'C' not in gamma and 'D' not in gamma and 'F' not in gamma and 'G' not in gamma
        keySignature = MajorMinor('E', 'C#')
    elif blacks == ['Ab', 'Bb', 'C#', 'Eb']:
        assert 'B' not in gamma and 'E' not in gamma and 'A' not in gamma and 'D' not in gamma
        keySignature = MajorMinor('Ab', 'F')
    else: assert False

elif 'B' in gamma and 'E' in gamma: keySignature = MajorMinor('B', 'Ab')
elif 'C' in gamma and 'F' in gamma: keySignature = MajorMinor('C#', 'Bb')
else: assert False

print(keySignature)
)", main_namespace));

		// 9 Finally, Write MIDI
		const auto pyScript9(p::exec(R"(
import mido

microSecPerBeat, ppqn = mido.bpm2tempo(lbr.beat.tempo(song).mean()), 480
midi, track = mido.MidiFile(ticks_per_beat=ppqn), mido.MidiTrack()
midi.tracks.append(track)
track.append(mido.MetaMessage('text',          text  = 'Automatically transcribed from audio:\r\n\t' + songName))
track.append(mido.MetaMessage('copyright',     text  = 'Used software created by Boris Shakhovsky'))
track.append(mido.MetaMessage('set_tempo',     tempo = microSecPerBeat))
track.append(mido.MetaMessage('key_signature', key   = keySignature))

for i, note in enumerate(np.argwhere(yPred[0]).ravel()):
    track.append(mido.Message('note_on', note=note+21, velocity=int(0x7F * yProb[frames[0]][note])))
for i, fr in enumerate(frames[1:]):
    track.append(mido.Message('note_off', note=0, time=int(mido.second2tick(
        lbr.frames_to_time(fr) - lbr.frames_to_time(frames[i]), ppqn, microSecPerBeat))))
    for note in np.argwhere(yPred[i]).ravel():
        track.append(mido.Message('note_off', note=note+21))
    for note in np.argwhere(yPred[i + 1]).ravel():
        track.append(mido.Message('note_on', note=note+21, velocity=int(0x7F * yProb[fr][note])))

midiOutFile = '.'.join(songName.split('.')[:-1]) + '.mid'
midi.save(midiOutFile)
print('"{}" saved'.format(midiOutFile))
)", main_namespace));
	}
	catch (const p::error_already_set&)
	{
		if (PyErr_ExceptionMatches(PyExc_AssertionError)) cout << "Python assertion failed" << endl;
		PyErr_Print();
	}

	system("Pause");
}