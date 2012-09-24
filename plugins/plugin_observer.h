// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PLUGIN_OBSERVER_H_
#define CHROME_BROWSER_PLUGINS_PLUGIN_OBSERVER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/tab_contents/web_contents_user_data.h"
#include "content/public/browser/web_contents_observer.h"

#if defined(ENABLE_PLUGIN_INSTALLATION)
#include <map>
#endif

class GURL;
class InfoBarDelegate;
class PluginFinder;

#if defined(ENABLE_PLUGIN_INSTALLATION)
class PluginInstaller;
class PluginPlaceholderHost;
#endif

namespace content {
class WebContents;
}

class PluginObserver : public content::WebContentsObserver,
                       public WebContentsUserData<PluginObserver> {
 public:
  virtual ~PluginObserver();

  // content::WebContentsObserver implementation.
  virtual void PluginCrashed(const FilePath& plugin_path) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

#if defined(ENABLE_PLUGIN_INSTALLATION)
  void InstallMissingPlugin(PluginInstaller* installer);
#endif

  // Make public the web_contents() accessor that is protected in the parent.
  using content::WebContentsObserver::web_contents;

 private:
  explicit PluginObserver(content::WebContents* web_contents);
  static int kUserDataKey;
  friend class WebContentsUserData<PluginObserver>;

  class PluginPlaceholderHost;

  void OnBlockedUnauthorizedPlugin(const string16& name,
                                   const std::string& identifier);
  void OnBlockedOutdatedPlugin(int placeholder_id,
                               const std::string& identifier);
#if defined(ENABLE_PLUGIN_INSTALLATION)
  void OnFindMissingPlugin(int placeholder_id, const std::string& mime_type);

  void FindMissingPlugin(int placeholder_id,
                         const std::string& mime_type,
                         PluginFinder* plugin_finder);
  void FindPluginToUpdate(int placeholder_id,
                          const std::string& identifier,
                          PluginFinder* plugin_finder);
  void OnRemovePluginPlaceholderHost(int placeholder_id);
#endif
  void OnOpenAboutPlugins();
  void OnCouldNotLoadPlugin(const FilePath& plugin_path);

  base::WeakPtrFactory<PluginObserver> weak_ptr_factory_;

#if defined(ENABLE_PLUGIN_INSTALLATION)
  // Stores all PluginPlaceholderHosts, keyed by their routing ID.
  std::map<int, PluginPlaceholderHost*> plugin_placeholders_;
#endif

  DISALLOW_COPY_AND_ASSIGN(PluginObserver);
};

#endif  // CHROME_BROWSER_PLUGINS_PLUGIN_OBSERVER_H_
