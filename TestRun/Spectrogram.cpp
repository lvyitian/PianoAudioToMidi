#include "stdafx.h"
#include "Spectrogram.h"
#include "MainWindow.h"

#include "CanvasGdi.h"
#include "BitmapCompatible.h"
#include "CursorWait.h"

#include "Resource.h"

#include "PianoToMidi.h"

using namespace std;

HWND	Spectrogram::spectrTitle	= nullptr, Spectrogram::spectr	= nullptr,
		Spectrogram::spectrLog		= nullptr, Spectrogram::progBar	= nullptr,
		Spectrogram::calcSpectr		= nullptr, Spectrogram::convert = nullptr;

LONG	Spectrogram::calcSpectrWidth	= 0,	Spectrogram::calcSpectrHeight	= 0,
		Spectrogram::spectrTitleWidth	= 0,	Spectrogram::spectrTitleHeight	= 0,
		Spectrogram::spectrWidth		= 0,	Spectrogram::spectrHeight		= 0,
		Spectrogram::convetWidth		= 0,	Spectrogram::convertHeight		= 0,
		Spectrogram::progBarHeight		= 0;

LPCTSTR	Spectrogram::mediaFile = nullptr;
std::unique_ptr<PianoToMidi> Spectrogram::media = nullptr;
std::string Spectrogram::log = "";
bool Spectrogram::toRepaint = true, Spectrogram::midiWritten = false;

BOOL Spectrogram::OnInitDialog(const HWND hDlg, const HWND, const LPARAM)
{
	spectrTitle	= GetDlgItem(hDlg, IDC_SPECTR_TITLE);
	spectr		= GetDlgItem(hDlg, IDC_SPECTRUM);
	spectrLog	= GetDlgItem(hDlg, IDC_SPECTR_LOG);
	progBar		= GetDlgItem(hDlg, IDC_CNN_PROG);
	calcSpectr	= GetDlgItem(hDlg, IDB_CALC_SPECTR);
	convert		= GetDlgItem(hDlg, IDB_CONVERT);

	RECT rect;
	GetWindowRect(calcSpectr, &rect);
	calcSpectrWidth = rect.right - rect.left;
	calcSpectrHeight = rect.bottom - rect.top;

	GetWindowRect(spectrTitle, &rect);
	spectrTitleWidth = rect.right - rect.left;
	spectrTitleHeight = rect.bottom - rect.top;

	GetWindowRect(convert, &rect);
	convetWidth = rect.right - rect.left;
	convertHeight = rect.bottom - rect.top;

	GetWindowRect(progBar, &rect);
	progBarHeight = rect.bottom - rect.top;

	ShowWindow(hDlg, SW_SHOWMAXIMIZED);

//	assert(mediaFile and "Unknown audio file name");
	media = make_unique<PianoToMidi>();
	log.clear();
	toRepaint = true;
	midiWritten = false;
	
#ifdef UNICODE
	const wstring wFile(mediaFile);
	const string aFile(wFile.cbegin(), wFile.cend());
#else
	const string aFile(mediaFile);
#endif
	try
	{
		log += media->FFmpegDecode(aFile.c_str());
		log = regex_replace(log, regex("\r?\n\r?"), "\r\n");
		log += "\r\n";
		SetWindowTextA(spectrLog, log.c_str());
	}
	catch (const FFmpegError& e)
	{
		SetWindowTextA(spectrLog, e.what());
		MessageBoxA(hDlg, e.what(), "Audio file error", MB_OK | MB_ICONHAND);
		return true;
	}

	Button_Enable(calcSpectr, true);

	return true;
}

void Spectrogram::OnSize(const HWND hDlg, const UINT, const int cx, const int cy)
{
	SetWindowPos(calcSpectr, nullptr,
		(cx - calcSpectrWidth) / 2, edge,
		0, 0, SWP_NOSIZE | SWP_NOZORDER);

	SetWindowPos(spectrTitle, nullptr,
		(cx - spectrTitleWidth) / 2, calcSpectrHeight + 2 * edge,
		0, 0, SWP_NOSIZE | SWP_NOZORDER);
	
	spectrWidth = cx - 2 * edge;
	spectrHeight = cy / 2 - calcSpectrHeight - spectrTitleHeight - 3 * edge;
	SetWindowPos(spectr, nullptr,
		edge, calcSpectrHeight + spectrTitleHeight + 3 * edge,
		spectrWidth, spectrHeight, SWP_NOZORDER);
	
	SetWindowPos(convert, nullptr,
		(cx - convetWidth) / 2, cy / 2 + edge,
		0, 0, SWP_NOSIZE | SWP_NOZORDER);
	
	SetWindowPos(progBar, nullptr, edge,
		cy / 2 + convertHeight + 2 * edge,
		cx - 2 * edge, progBarHeight, SWP_NOZORDER);
	
	SetWindowPos(spectrLog, nullptr, edge,
		cy / 2 + convertHeight + progBarHeight + 3 * edge, cx - 2 * edge,
		cy / 2 - convertHeight - progBarHeight - 4 * edge, SWP_NOZORDER);

	InvalidateRect(hDlg, nullptr, true);
}

void Spectrogram::OnPaint(const HWND hDlg)
{
	CanvasGdi canvas(hDlg);
	if (media->GetCqt().empty()) return;

	const auto cqt(media->GetCqt());
	const auto cqtSize(min(static_cast<size_t>(spectrWidth), cqt.size() / media->GetNumBins()));
	wostringstream wos;
	if (cqtSize == static_cast<size_t>(spectrWidth)) wos << TEXT("SPECTROGRAM OF THE FIRST ")
		<< media->GetMidiSeconds() * cqtSize * media->GetNumBins() / cqt.size() << TEXT(" SECONDS:");
	else wos << TEXT("SPECTROGRAM OF ALL THE ") << media->GetMidiSeconds() << TEXT(" SECONDS:");
	Static_SetText(spectrTitle, wos.str().c_str());

	if (not toRepaint) return;

	const auto binWidth(spectrWidth / static_cast<LONG>(cqtSize));
	const auto binsPerPixel((media->GetNumBins() - 1) / spectrHeight + 1);
	const auto cqtMax(*max_element(cqt.cbegin(), cqt.cbegin()
		+ static_cast<ptrdiff_t>(cqtSize * media->GetNumBins())));

	const BitmapCompatible hBitmap(spectr, spectrWidth, spectrHeight);

	const auto bottom((spectrHeight + (media->GetNumBins() / binsPerPixel)) / 2);
	for (size_t i(0); i < cqtSize; ++i)
		for (size_t j(0); j < media->GetNumBins() / binsPerPixel; ++j)
		{
			const auto bin(accumulate(cqt.cbegin() + static_cast<ptrdiff_t>(i * media->GetNumBins()
				+ j * binsPerPixel), cqt.cbegin() + static_cast<ptrdiff_t>(i * media->GetNumBins()
					+ (j + 1) * binsPerPixel), 0.f) * 5 / cqtMax / binsPerPixel);
//			assert(bin >= 0 and bin <= 5 and "Wrong cqt-bin value");
			SetPixelV(hBitmap, static_cast<int>(i), static_cast<int>(bottom - j),
				bin < 1 ? RGB(0, 0, bin * 0xFF) : bin < 2 ? RGB(0, (bin - 1) * 0xFF, 0xFF) :
				bin < 3 ? RGB(0, 0xFF, 0xFF * (3 - bin)) : bin < 4
				? RGB((bin - 4) * 0xFF, 0xFF, 0) : RGB(0xFF, 0xFF * (5 - bin), 0));
		}

	// TODO: Draw notes
}

void Spectrogram::OnCommand(const HWND hDlg, const int id, const HWND hCtrl, const UINT)
{
	switch (id)
	{
	case IDOK: case IDCANCEL:
		EndDialog(hDlg, id);
		if (not midiWritten) return;
		break;

	case IDB_CALC_SPECTR:
		Button_Enable(hCtrl, false);
		{
			CursorWait cursor;

			try
			{
				log += media->CqtTotal() + "\r\n";
				log = regex_replace(log, regex("\r?\n\r?"), "\r\n"); // just in case
				SetWindowTextA(spectrLog, log.c_str());
				InvalidateRect(hDlg, nullptr, true);
			}
			catch (const CqtError& e)
			{
				log += string("\r\n") + e.what();
				log = regex_replace(log, regex("\r?\n\r?"), "\r\n"); // just in case
				SetWindowTextA(spectrLog, log.c_str());
				MessageBoxA(hDlg, e.what(), "Spectrogram error", MB_OK | MB_ICONHAND);
				break;
			}

			try
			{
				log += media->HarmPerc() + "\r\n";
				log = regex_replace(log, regex("\r?\n\r?"), "\r\n"); // just in case
				SetWindowTextA(spectrLog, log.c_str());
			}
			catch (const CqtError& e)
			{
				log += string("\r\n") + e.what();
				log = regex_replace(log, regex("\r?\n\r?"), "\r\n"); // just in case
				SetWindowTextA(spectrLog, log.c_str());
				MessageBoxA(hDlg, e.what(), "Harmonic-Percussive separation error", MB_OK | MB_ICONHAND);
				break;
			}

			try
			{
				log += media->Tempo() + "\r\n";
				log = regex_replace(log, regex("\r?\n\r?"), "\r\n"); // just in case
				SetWindowTextA(spectrLog, log.c_str());
			}
			catch (const CqtError& e)
			{
				log += string("\r\n") + e.what();
				log = regex_replace(log, regex("\r?\n\r?"), "\r\n"); // just in case
				SetWindowTextA(spectrLog, log.c_str());
				MessageBoxA(hDlg, e.what(), "Tempogram error", MB_OK | MB_ICONHAND);
				break;
			}

			try
			{
#ifdef UNICODE
				const wstring pathW(MainWindow::path);
				const string pathA(pathW.cbegin(), pathW.cend());
#else
				const string pathA(MainWindow::path);
#endif
				log += "\r\n" + media->KerasLoad(pathA) + "\r\n";
				log = regex_replace(log, regex("\r?\n\r?"), "\r\n"); // here it is required, because two lines
				SetWindowTextA(spectrLog, log.c_str());
			}
			catch (const KerasError& e)
			{
				log += string("\r\n") + e.what();
				log = regex_replace(log, regex("\r?\n\r?"), "\r\n"); // just in case
				SetWindowTextA(spectrLog, log.c_str());
				MessageBoxA(hDlg, e.what(), "Neural network file error", MB_OK | MB_ICONHAND);
				break;
			}

			Button_Enable(convert, true);
		}
		break;

	case IDB_CONVERT:
		Button_Enable(hCtrl, false);
		{
			CursorWait cursor;

			try
			{
				// Consider using 'GetTickCount64' : GetTickCount overflows every 49 days,
				// and code can loop indefinitely
#pragma warning(suppress:28159)
				auto timeStart(GetTickCount());
				for (WPARAM percent(0); percent < 100u; percent = media->CnnProbabs())
				{
					SendMessage(progBar, PBM_SETPOS, percent, 0);
					// Consider using 'GetTickCount64' : GetTickCount overflows every 49 days,
					// and code can loop indefinitely
#pragma warning(suppress:28159)
					if (static_cast<int>(GetTickCount()) - static_cast<int>(timeStart) > 10'000)
						if (MessageBox(hDlg,
							TEXT("It seems that conversion might take a while.\n")
							TEXT("Press OK if you want to continue waiting."),
							TEXT("Neural net in process..."),
							MB_ICONQUESTION | MB_OKCANCEL | MB_DEFBUTTON2) == IDCANCEL)
						{
							EndDialog(hDlg, id);
							return;
						}
						else timeStart = USER_TIMER_MAXIMUM;
				}
			}
			catch (const KerasError& e)
			{
				log += string("\r\n") + e.what();
				log = regex_replace(log, regex("\r?\n\r?"), "\r\n"); // just in case
				SetWindowTextA(spectrLog, log.c_str());
				MessageBoxA(hDlg, e.what(), "Neural network forward pass error", MB_OK | MB_ICONHAND);
				break;
			}
			InvalidateRect(hDlg, nullptr, true);
			SendMessage(progBar, PBM_SETPOS, 99, 0);

			try { log += "\r\n" + media->Gamma() + "\r\n"; }
			catch (const KerasError& e)
			{
				log += string("\n") + e.what();
				log = regex_replace(log, regex("\r?\n\r?"), "\r\n"); // just in case
				SetWindowTextA(spectrLog, log.c_str());
				MessageBoxA(hDlg, e.what(), "Key signature error", MB_OK | MB_ICONHAND);
				midiWritten = true;
				break;
			}

			log += media->KeySignature() + "\r\n";
			log = regex_replace(log, regex("\r?\n\r?"), "\r\n"); // just in case
			SetWindowTextA(spectrLog, log.c_str());

			try
			{
				// TODO: file-open dialog box to allow for choosing output MIDI-file name
//				media->WriteMidi("../../Test songs/Test.mid");
				log += "\r\nMIDI written.";
				log = regex_replace(log, regex("\r?\n\r?"), "\r\n"); // just in case
				SetWindowTextA(spectrLog, log.c_str());
				midiWritten = true;
				SendMessage(progBar, PBM_SETPOS, 100, 0);
			}
			catch (const MidiOutError& e)
			{
				log += string("\r\n") + e.what();
				SetWindowTextA(spectrLog, log.c_str());
				MessageBoxA(hDlg, e.what(), "MIDI write error", MB_OK | MB_ICONHAND);
			}
		}
	}
}

INT_PTR CALLBACK Spectrogram::Main(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		HANDLE_MSG(hDlg, WM_INITDIALOG, OnInitDialog);
		HANDLE_MSG(hDlg, WM_SIZE,		OnSize);
		HANDLE_MSG(hDlg, WM_PAINT,		OnPaint);
		HANDLE_MSG(hDlg, WM_COMMAND,	OnCommand);
	case WM_ENTERSIZEMOVE:	toRepaint = false; break;
	case WM_EXITSIZEMOVE:	toRepaint = true; InvalidateRect(hDlg, nullptr, true);
	}
	return false;
}