#ifndef CD3DAPP_H
#define CD3DAPP_H

#include <string>
#include "../Graphics_Lib/cBaseD3D.h"
#include "../Utilities/My_Timer.h"

class cLoginControl;
class cDemoControl;

namespace MY_UI{
	namespace Utilities{
		class cInput;
	};
	namespace Controls{
		class Text;
	};
};
class UI_Camera;

class D3DApp: public cBaseD3D {
public:
	D3DApp(HINSTANCE hInstance, std::string appname = "Ocean Demo", unsigned int height = 768, unsigned int width = 1024);
	virtual ~D3DApp();

	void run();

	void UpdateStats();

	virtual LRESULT msgProc(unsigned int msg, WPARAM wParam, LPARAM lParam);

	std::string FrameStats;
	UI_Camera* UICamera;
	My_Timer::Frame_Timer FrameTimer;
	MY_UI::Utilities::cInput *Input;
	MY_UI::Controls::Text* Frame_Stats;
};

#endif
