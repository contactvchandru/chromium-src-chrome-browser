// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/web_data_service_factory.h"

#include "base/files/file_path.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/webdata/autofill_web_data_service_impl.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/browser/webdata/web_database_service.h"
#include "chrome/browser/webdata/web_database_service_factory.h"
#include "chrome/common/chrome_constants.h"

// static
scoped_ptr<AutofillWebDataService> AutofillWebDataService::FromBrowserContext(
    content::BrowserContext* context) {
  // For this service, the implicit/explicit distinction doesn't
  // really matter; it's just used for a DCHECK.  So we currently
  // cheat and always say EXPLICIT_ACCESS.
  scoped_refptr<WebDataService> service = WebDataServiceFactory::GetForProfile(
      static_cast<Profile*>(context), Profile::EXPLICIT_ACCESS);

  if (service.get()) {
    return scoped_ptr<AutofillWebDataService>(
        new AutofillWebDataServiceImpl(service));
  } else {
    return scoped_ptr<AutofillWebDataService>(NULL);
  }
}

// static
scoped_refptr<WebDataService> WebDataService::FromBrowserContext(
    content::BrowserContext* context) {
  // For this service, the implicit/explicit distinction doesn't
  // really matter; it's just used for a DCHECK.  So we currently
  // cheat and always say EXPLICIT_ACCESS.
  return WebDataServiceFactory::GetForProfile(
      static_cast<Profile*>(context), Profile::EXPLICIT_ACCESS);
}

WebDataServiceFactory::WebDataServiceFactory()
    : RefcountedProfileKeyedServiceFactory(
          "WebDataService",
          ProfileDependencyManager::GetInstance()) {
  DependsOn(WebDatabaseServiceFactory::GetInstance());
}

WebDataServiceFactory::~WebDataServiceFactory() {}

// static
scoped_refptr<WebDataService> WebDataServiceFactory::GetForProfile(
    Profile* profile, Profile::ServiceAccessType access_type) {
  // If |access_type| starts being used for anything other than this
  // DCHECK, we need to start taking it as a parameter to
  // AutofillWebDataServiceImpl::FromBrowserContext (see above).
  DCHECK(access_type != Profile::IMPLICIT_ACCESS || !profile->IsOffTheRecord());
  return static_cast<WebDataService*>(
      GetInstance()->GetServiceForProfile(profile, true).get());
}

// static
WebDataServiceFactory* WebDataServiceFactory::GetInstance() {
  return Singleton<WebDataServiceFactory>::get();
}

bool WebDataServiceFactory::ServiceRedirectedInIncognito() const {
  return true;
}

scoped_refptr<RefcountedProfileKeyedService>
WebDataServiceFactory::BuildServiceInstanceFor(Profile* profile) const {
  DCHECK(profile);
  scoped_refptr<WebDataService> wds(new WebDataService(
      WebDatabaseService::FromBrowserContext(profile)));
  wds->Init();
  return wds.get();
}

bool WebDataServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
