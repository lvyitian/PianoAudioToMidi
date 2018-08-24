#include "stdafx.h"
#include "PianoToMidi.h"

using namespace std;

#define CATCH(ERROR) catch (const ERROR& e) { cout << e.what() << endl; system("Pause"); return 1; }

int main()
{
	const PianoToMidi media;

	try
	{
		cout << media.FFmpegDecode(
			"C:/Users/Boris/Documents/Visual Studio 2017/Projects/Piano 2 Midi/Test songs/"
//			"chpn_op7_1MINp_align.wav"
			"MAPS_MUS-chpn-p7_SptkBGCl.wav"
//			"Eric Clapton - Wonderful Tonight.wma"
//			"Chopin_op10_no3_p05_aligned.mp3"
		).c_str() << endl;
	}
	CATCH(FFmpegError);

	try { cout << media.CqtTotal().c_str() << endl; } CATCH(CqtError);
	try { cout << media.HarmPerc().c_str() << endl; } CATCH(CqtError);
	try { cout << media.Tempo().c_str() << endl; } CATCH(CqtError);

	try { cout << endl << media.KerasLoad().c_str() << endl; } CATCH(KerasError);
	try
	{
		for (int percent(0); percent < 100; percent = media.CnnProbabs()) cout << percent << "%\r";
		cout << 100 << "%" << endl;
	}
	CATCH(KerasError);

	try { cout << media.Gamma().c_str() << endl; } CATCH(KerasError);
	cout << media.KeySignature().c_str() << endl;

	try { media.WriteMidi("../../Test songs/Test.mid"); } CATCH(MidiOutError);

	system("Pause");
}