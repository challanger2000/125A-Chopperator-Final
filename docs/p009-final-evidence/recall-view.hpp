#pragma once

#include "EditorPreferences.hpp"
#include "ValueEntry.hpp"
#include "public.sdk/source/common/pluginview.h"
#include "public.sdk/source/vst/vsteditcontroller.h"

namespace chopper {
// Substitute only the platform editor at construction. The controller and
// its notifications are the product bodies copied unmodified from Git.
class Editor final : public Steinberg::CPluginView {
public:
    Editor(Steinberg::Vst::EditController*,EditorPreferences& prefs) : prefs(prefs) {}
    EditorPreferences& prefs;
};
}
