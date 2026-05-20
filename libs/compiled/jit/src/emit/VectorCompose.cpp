// VectorCompose.cpp
//
// IR emission for VectorCompose. Per-port queue state and try_sync are
// reused verbatim from Join (see emit/Join.cpp). The only operator-specific
// behaviour is that the synced N scalars are written to N consecutive output
// slots downstream of the Output op rather than being plumbed back through
// the per-port value_map. That post-sync slot write lives in SegmentEmitter.
//
// This translation unit exists so the inline wrappers in
// rtbot/compiled/jit/emit/VectorCompose.h can be linked from the JIT lib
// even when no caller currently takes their address.

#include "rtbot/compiled/jit/emit/VectorCompose.h"
