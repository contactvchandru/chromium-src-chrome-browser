// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_TYPES_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_TYPES_H_

// A particular type of content to care about.  We give the user various types
// of controls over each of these.
enum ContentSettingsType {
  // "DEFAULT" is only used as an argument to the Content Settings Window
  // opener; there it means "whatever was last shown".
  CONTENT_SETTINGS_TYPE_DEFAULT = -1,
  CONTENT_SETTINGS_FIRST_TYPE = 0,
  CONTENT_SETTINGS_TYPE_COOKIES = CONTENT_SETTINGS_FIRST_TYPE,
  CONTENT_SETTINGS_TYPE_IMAGES,
  CONTENT_SETTINGS_TYPE_JAVASCRIPT,
  CONTENT_SETTINGS_TYPE_PLUGINS,
  CONTENT_SETTINGS_TYPE_POPUPS,
  CONTENT_SETTINGS_NUM_TYPES
};

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_TYPES_H_

