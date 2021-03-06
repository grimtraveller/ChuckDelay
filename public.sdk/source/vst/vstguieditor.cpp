//------------------------------------------------------------------------
// Project     : VST SDK
// Version     : 3.5.2
//
// Category    : Helpers
// Filename    : public.sdk/source/vst/vstguieditor.h
// Created by  : Steinberg, 04/2005
// Description : VSTGUI Editor
//
//-----------------------------------------------------------------------------
// LICENSE
// (c) 2012, Steinberg Media Technologies GmbH, All Rights Reserved
//-----------------------------------------------------------------------------
// This Software Development Kit may not be distributed in parts or its entirety  
// without prior written agreement by Steinberg Media Technologies GmbH. 
// This SDK must not be used to re-engineer or manipulate any technology used  
// in any Steinberg or Third-party application or software module, 
// unless permitted by law.
// Neither the name of the Steinberg Media Technologies nor the names of its
// contributors may be used to endorse or promote products derived from this 
// software without specific prior written permission.
// 
// THIS SDK IS PROVIDED BY STEINBERG MEDIA TECHNOLOGIES GMBH "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL STEINBERG MEDIA TECHNOLOGIES GMBH BE LIABLE FOR ANY DIRECT, 
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.
//-----------------------------------------------------------------------------

#include "vstguieditor.h"
#include "pluginterfaces/base/keycodes.h"

#if VSTGUI_VERSION_MAJOR < 4
#include "vstgui/vstkeycode.h"
#endif

#include "base/source/fstring.h"

#if MAC
#include <dlfcn.h>
namespace VSTGUI {
	static void CreateVSTGUIBundleRef ();
	static void ReleaseVSTGUIBundleRef ();
}
#elif WINDOWS
void* hInstance = 0; // VSTGUI hInstance
extern void* moduleHandle;

#endif // MAC

namespace Steinberg {
namespace Vst {

//------------------------------------------------------------------------
VSTGUIEditor::VSTGUIEditor (void* controller, ViewRect* size) 
: EditorView (static_cast<EditController*>(controller), size) 
{
	#if MAC
	CreateVSTGUIBundleRef ();
	#elif WINDOWS
	hInstance = moduleHandle;
	#endif
	// create a timer used for idle update: will call notify method
	timer = new CVSTGUITimer (dynamic_cast<CBaseObject*>(this));
}

//------------------------------------------------------------------------
VSTGUIEditor::~VSTGUIEditor ()
{
	if (timer)
		timer->forget ();
	#if MAC
	ReleaseVSTGUIBundleRef ();
	#endif
}

//------------------------------------------------------------------------
void VSTGUIEditor::setIdleRate (int32 millisec)
{
	if (timer)
		timer->setFireTime (millisec);
}

//------------------------------------------------------------------------
tresult PLUGIN_API VSTGUIEditor::isPlatformTypeSupported (FIDString type)
{
#if WINDOWS
	if (strcmp (type, kPlatformTypeHWND) == 0)
		return kResultTrue;

#elif MAC
	#if MAC_CARBON
	if (strcmp (type, kPlatformTypeHIView) == 0)
		return kResultTrue;
	#endif

	#if MAC_COCOA
	if (strcmp (type, kPlatformTypeNSView) == 0)
		return kResultTrue;
	#endif
#endif
	
	return kInvalidArgument;
}

//------------------------------------------------------------------------
tresult PLUGIN_API VSTGUIEditor::attached (void* parent, FIDString type)
{
#if MAC
	if (isPlatformTypeSupported (type) != kResultTrue)
		return kResultFalse;

	#if MAC_COCOA && MAC_CARBON && !(VSTGUI_VERSION_MAJOR >= 4 && VSTGUI_VERSION_MINOR >= 1)
	CFrame::setCocoaMode (strcmp (type, kPlatformTypeNSView) == 0);
	#endif
#endif

	if (timer)
		timer->start ();

#if VSTGUI_VERSION_MAJOR >= 4 && VSTGUI_VERSION_MINOR >= 1
	PlatformType platformType = kDefaultNative;
#if MAC
	#if MAC_CARBON
	if (strcmp (type, kPlatformTypeHIView) == 0)
		platformType = kWindowRef;
	#endif
	
	#if MAC_COCOA
	if (strcmp (type, kPlatformTypeNSView) == 0)
		platformType = kNSView;
	#endif
#endif	
	if (open (parent, platformType) == true)
#else
	if (open (parent) == true)
#endif
	{
		ViewRect vr (0, 0, (int32)frame->getWidth (), (int32)frame->getHeight ());
		setRect (vr);
		if (plugFrame)
			plugFrame->resizeView (this, &vr);
	}
	return EditorView::attached (parent, type);
}

//------------------------------------------------------------------------
tresult PLUGIN_API VSTGUIEditor::removed ()
{
	if (timer)
		timer->stop ();

	close ();
	return EditorView::removed ();
}

//------------------------------------------------------------------------
tresult PLUGIN_API VSTGUIEditor::onSize (ViewRect* newSize)
{
	if (frame)
		frame->setSize (newSize->right - newSize->left, newSize->bottom - newSize->top);

	return EditorView::onSize (newSize);
}

//------------------------------------------------------------------------
void VSTGUIEditor::beginEdit (VSTGUI_INT32 index)
{
	if (controller)
		controller->beginEdit (index);
}

//------------------------------------------------------------------------
void VSTGUIEditor::endEdit (VSTGUI_INT32 index)
{
	if (controller)
		controller->endEdit (index);
}

//------------------------------------------------------------------------
VSTGUI_INT32 VSTGUIEditor::getKnobMode () const
{
	switch (EditController::getHostKnobMode ())
	{
		case kRelativCircularMode: return VSTGUI::kRelativCircularMode;
		case kLinearMode: return VSTGUI::kLinearMode;
	}
	return VSTGUI::kCircularMode;
}

//------------------------------------------------------------------------
CMessageResult VSTGUIEditor::notify (CBaseObject* /*sender*/, const char* message)
{
	if (message == CVSTGUITimer::kMsgTimer)
	{
		if (frame)
 			frame->idle ();
 
 		return kMessageNotified;
 	}
 
 	return kMessageUnknown; 
}

//------------------------------------------------------------------------
static bool translateKeyMessage (VstKeyCode& keyCode, char16 key, int16 keyMsg, int16 modifiers)
{
	keyCode.character = 0;
	keyCode.virt = (unsigned char)keyMsg;
	keyCode.modifier = 0;
	if (key == 0)
		key = VirtualKeyCodeToChar ((uint8)keyMsg);
	if (key)
	{
		String keyStr (STR(" "));
		keyStr.setChar16 (0, key);
		keyStr.toMultiByte (kCP_Utf8);
		if (keyStr.length () == 1)
			keyCode.character = keyStr.getChar8 (0);
	}
	if (modifiers)
	{
		if (modifiers & kShiftKey)
			keyCode.modifier |= MODIFIER_SHIFT;
		if (modifiers & kAlternateKey)
			keyCode.modifier |= MODIFIER_ALTERNATE;
		if (modifiers & kCommandKey)
			keyCode.modifier |= MODIFIER_CONTROL;
		if (modifiers & kControlKey)
			keyCode.modifier |= MODIFIER_COMMAND;
	}
	return true;
}

//------------------------------------------------------------------------
tresult PLUGIN_API VSTGUIEditor::onKeyDown (char16 key, int16 keyMsg, int16 modifiers)
{
	if (frame)
	{
		VstKeyCode keyCode = {0};
		if (translateKeyMessage (keyCode, key, keyMsg, modifiers))
		{
			VSTGUI_INT32 result = frame->onKeyDown (keyCode);
			if (result == 1)
				return kResultTrue;
		}
	}
	return kResultFalse;
}

//------------------------------------------------------------------------
tresult PLUGIN_API VSTGUIEditor::onKeyUp (char16 key, int16 keyMsg, int16 modifiers)
{
	if (frame)
	{
		VstKeyCode keyCode = {0};
		if (translateKeyMessage (keyCode, key, keyMsg, modifiers))
		{
			VSTGUI_INT32 result = frame->onKeyUp (keyCode);
			if (result == 1)
				return kResultTrue;
		}
	}
	return kResultFalse;
}

//------------------------------------------------------------------------
tresult PLUGIN_API VSTGUIEditor::onWheel (float distance)
{
	if (frame)
	{
		CPoint where;
		frame->getCurrentMouseLocation (where);
		if (frame->onWheel (where, distance, frame->getCurrentMouseButtons ()))
			return kResultTrue;
	}
	return kResultFalse;
}

//------------------------------------------------------------------------
} // namespace Vst
} // namespace Steinberg

#if MAC
namespace VSTGUI {
void* gBundleRef = 0;
static int openCount = 0;
//------------------------------------------------------------------------
void CreateVSTGUIBundleRef ()
{
	openCount++;
	if (gBundleRef)
	{
		CFRetain (gBundleRef);
		return;
	}
	Dl_info info;
	if (dladdr ((const void*)CreateVSTGUIBundleRef, &info))
	{
		if (info.dli_fname)
		{
			Steinberg::String name;
			name.assign (info.dli_fname);
			for (int i = 0; i < 3; i++)
			{
				int delPos = name.findLast ('/');
				if (delPos == -1)
				{
					fprintf (stdout, "Could not determine bundle location.\n");
					return; // unexpected
				}
				name.remove (delPos, name.length () - delPos);
			}
			CFURLRef bundleUrl = CFURLCreateFromFileSystemRepresentation (0, (const UInt8*)name.text8 (), name.length (), true);
			if (bundleUrl)
			{
				gBundleRef = CFBundleCreate (0, bundleUrl);
				CFRelease (bundleUrl);
			}
		}
	}
}

//------------------------------------------------------------------------
void ReleaseVSTGUIBundleRef ()
{
	openCount--;
	if (gBundleRef)
		CFRelease (gBundleRef);
	if (openCount == 0)
		gBundleRef = 0;
}

//------------------------------------------------------------------------
} // namespace VSTGUI

#endif // MAC
