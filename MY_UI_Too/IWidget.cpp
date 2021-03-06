#include "PCH.h"
#include "IWidget.h"
#include "IRenderer.h"
#include "ISkin.h"
#include "IFont_Factory.h"
#include "IInput.h"

std::set<MY_UI_Too::Interfaces::IWidget*> MY_UI_Too::Internal::AllWidgets;
std::set<MY_UI_Too::Interfaces::IWidget*> MY_UI_Too::Internal::Widgets_ToBeDeleted;
MY_UI_Too::Interfaces::IWidget* MY_UI_Too::Internal::Root_Widget=nullptr;
MY_UI_Too::Interfaces::IRenderer* MY_UI_Too::Internal::Renderer=nullptr;
MY_UI_Too::Interfaces::ISkin* MY_UI_Too::Internal::UI_Skin=nullptr;
MY_UI_Too::Interfaces::IFont_Factory* MY_UI_Too::Internal::Font_Factory=nullptr;
MY_UI_Too::Interfaces::IInput* MY_UI_Too::Internal::Input=nullptr;

MY_UI_Too::Interfaces::IWidget* MY_UI_Too::Internal::Focus_Holder=nullptr;
MY_UI_Too::Interfaces::IWidget* MY_UI_Too::Internal::Hovered_Widget=nullptr;
MY_UI_Too::Interfaces::IWidget* MY_UI_Too::Internal::Dragged_Widget=nullptr;

MY_UI_Too::Interfaces::IWidget::IWidget(){
#if defined(DEBUG) || defined(_DEBUG)
	MY_UI_Too::Internal::AllWidgets.insert(this);
#endif
}
MY_UI_Too::Interfaces::IWidget::~IWidget(){
#if defined(DEBUG) || defined(_DEBUG)
	MY_UI_Too::Internal::AllWidgets.erase(this);
#endif
}

void MY_UI_Too::Init(MY_UI_Too::Interfaces::IRenderer* renderer, MY_UI_Too::Interfaces::ISkin* skin, MY_UI_Too::Interfaces::IFont_Factory* font_factory, MY_UI_Too::Interfaces::IWidget* root,  MY_UI_Too::Interfaces::IInput* input, unsigned int screen_width, unsigned int screen_height, unsigned int skinsize){
	Internal::Renderer=renderer; 
	Internal::UI_Skin = skin; 
	Internal::Root_Widget = root;

	Internal::Renderer->Init();
	Internal::Root_Widget->Set_Size(MY_UI_Too::Utilities::Point(screen_width, screen_height));

	Internal::UI_Skin->Init(Internal::Renderer, skinsize);

	Internal::Input = input;
	Internal::Input->SetCursor(Standard);
	Internal::Font_Factory = font_factory;
	Internal::Font_Factory->Init();
	Internal::Font_Factory->Get_Font();// to build the default font

} 
void MY_UI_Too::DeInit(){
	//object destruction is always opposite of the order it was created
	SAFE_DELETE(Internal::Root_Widget);
	
	SAFE_DELETE(Internal::UI_Skin);
	SAFE_DELETE(Internal::Renderer);

	SAFE_DELETE(Internal::Font_Factory);
	assert(MY_UI_Too::Internal::AllWidgets.empty());//if this failes it means that there are widgets which are not properly being deleted
}
