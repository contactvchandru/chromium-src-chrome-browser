// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/change_processor.h"
#include "chrome/browser/profile.h"

namespace browser_sync {

ChangeProcessor::~ChangeProcessor() {
  Stop();
}

void ChangeProcessor::Start(Profile* profile,
                            sync_api::UserShare* share_handle) {
  DCHECK(error_handler_ && !share_handle_);
  share_handle_ = share_handle;
  StartImpl(profile);
  running_ = true;
}

void ChangeProcessor::Stop() {
  if (!running_)
    return;
  StopImpl();
  share_handle_ = NULL;
  running_ = false;
}

}  // namespace browser_sync
