#pragma once

#include "MediaElement.h"

// Source-compatibility bridge for native callers. MediaElement is the only
// canonical runtime/XAML identity; this alias does not reintroduce a
// <MediaPlayer> schema entry.
using MediaPlayer [[deprecated("Use MediaElement")]] = MediaElement;
