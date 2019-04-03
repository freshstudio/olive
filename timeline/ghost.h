#ifndef GHOST_H
#define GHOST_H

#include "effects/transition.h"
#include "track.h"

namespace olive {
namespace timeline {

enum TrimType {
  TRIM_NONE,
  TRIM_IN,
  TRIM_OUT
};

}
}

struct Ghost {
  Clip* clip;
  long in;
  long out;
  Track* track;
  long clip_in;

  long old_in;
  long old_out;
  Track* old_track;
  long old_clip_in;

  // importing variables
  Media* media;
  int media_stream;

  // other variables
  long ghost_length;
  long media_length;
  olive::timeline::TrimType trim_type;

  // transition trimming
  TransitionPtr transition;

  Selection ToSelection() const;
};

#endif // GHOST_H