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

	assert(mediaFile and "Unknown audio file name");
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

void Spectrogram::OnDestroyDialog(const HWND)
{
	media = nullptr;
	log.clear();
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
			assert(bin >= 0 and bin <= 5 and "Wrong cqt-bin value");
			SetPixelV(hBitmap, static_cast<int>(i), static_cast<int>(bottom - j),
				bin < 1 ? RGB(0, 0, bin * 0xFF) : bin < 2 ? RGB(0, (bin - 1) * 0xFF, 0xFF) :
				bin < 3 ? RGB(0, 0xFF, 0xFF * (3 - bin)) : bin < 4
				? RGB((bin - 4) * 0xFF, 0xFF, 0) : RGB(0xFF, 0xFF * (5 - bin), 0));
		}

	if (media->GetNotes().empty()) return;

	for (size_t i(0); i < media->GetOnsetFrames().size(); ++i)
	{
		const auto onset(media->GetOnsetFrames().at(i));
		if (onset > cqtSize) break;

		for (const auto& note : media->GetNotes().at(i))
		{
			const auto bin(static_cast<int>(bottom
				- note.first * media->GetNumBins() / 88 / binsPerPixel));
			SelectObject(hBitmap, GetStockPen(BLACK_PEN));
			SelectObject(hBitmap, GetStockBrush(WHITE_BRUSH));
			Ellipse(hBitmap, static_cast<int>(onset) - 5, bin - 5,
				static_cast<int>(onset) + 5, bin + 5);
		}
	}
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
				const auto timeStart(GetTickCount());
				const auto percentStart(media->CnnProbabs());
				bool alreadyAsked(false);
				for (auto percent(percentStart); percent < 100u; percent = media->CnnProbabs())
				{
					SendMessage(progBar, PBM_SETPOS, percent, 0);
					if (not alreadyAsked and percent >= percentStart + 1)
					{
						alreadyAsked = true;
						// Consider using 'GetTickCount64' : GetTickCount overflows every 49 days,
						// and code can loop indefinitely
#pragma warning(suppress:28159)
						const auto seconds((GetTickCount() - timeStart) * (100 - percent) / 1'000);
						ostringstream os;
						os << "Conversion will take " << seconds / 60 << " min : "
							<< seconds % 60 << " sec" << endl << "Press OK if you are willing to wait.";
						if (MessageBoxA(hDlg, os.str().c_str(), "Neural net in process...",
							MB_ICONQUESTION | MB_OKCANCEL | MB_DEFBUTTON1) == IDCANCEL)
						{
							Button_Enable(hCtrl, true);
							return;
						}
					}
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
			SendMessage(progBar, PBM_SETPOS, 99, 0);

			try { log += media->Gamma() + "\r\n"; }
			catch (const KerasError& e)
			{
				log += string("\n") + e.what();
				log = regex_replace(log, regex("\r?\n\r?"), "\r\n"); // just in case
				SetWindowTextA(spectrLog, log.c_str());
				MessageBoxA(hDlg, e.what(), "Key signature error", MB_OK | MB_ICONHAND);
				midiWritten = true;
				break;
			}
			InvalidateRect(hDlg, nullptr, true);

			log += media->KeySignature() + "\r\n";
			log = regex_replace(log, regex("\r?\n\r?"), "\r\n"); // just in case
			SetWindowTextA(spectrLog, log.c_str());

			OPENFILENAME fileName{ sizeof fileName, hDlg };
			fileName.lpstrFilter = TEXT("MIDI files (*.mid)\0")	TEXT("*.mid*\0");
#ifdef UNICODE
			wstring
#else
			string
#endif
				fileT(mediaFile);
			auto fileNameIndex(fileT.find_last_of(TEXT("\\/")));
			if (fileNameIndex < fileT.length()) fileT.erase(0, fileNameIndex + 1);
			fileNameIndex = fileT.find_last_of('.');
			if (fileNameIndex < fileT.length()) fileT.erase(fileNameIndex);

			TCHAR buf[MAX_PATH];
#pragma warning(suppress:4189) // Local variable is initialized but not referenced
			const auto unusedIter(copy(fileT.cbegin(), fileT.cend(), buf));
			buf[fileT.length()] = 0;
			fileName.lpstrFile = buf;
			fileName.nMaxFile = sizeof buf / sizeof *buf;

			fileName.Flags = OFN_OVERWRITEPROMPT;
			while (not GetSaveFileName(&fileName)) if (MessageBox(hDlg,
				TEXT("MIDI-file will NOT be saved! Are you sure?!\n")
				TEXT("You will have to convert the audio again from the beginning."),
				TEXT("File save error"), MB_ICONHAND | MB_YESNO | MB_DEFBUTTON2) == IDYES)
			{
				log += "\r\nMIDI-file not saved :(";
				SetWindowTextA(spectrLog, log.c_str());
				return;
			}

#ifdef UNICODE
			const wstring fileW(fileName.lpstrFile);
			string fileA(fileW.cbegin(), fileW.cend());
#else
			string fileA(fileName.lpstrFile);
#endif
			if (fileA.length() < 4 or fileA.substr(fileA.length() - 4) != ".mid") fileA += ".mid";
			try
			{
				media->WriteMidi(fileA.c_str());
				log += "\r\n" + fileA + " saved.";
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
		HANDLE_MSG(hDlg, WM_DESTROY,	OnDestroyDialog);

		HANDLE_MSG(hDlg, WM_SIZE,		OnSize);
		HANDLE_MSG(hDlg, WM_PAINT,		OnPaint);
		HANDLE_MSG(hDlg, WM_COMMAND,	OnCommand);

	case WM_ENTERSIZEMOVE:	toRepaint = false; break;
	case WM_EXITSIZEMOVE:	toRepaint = true; InvalidateRect(hDlg, nullptr, true);
	}
	return false;
}