// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_PROFILE_SYNC_SERVICE_H_
#define CHROME_BROWSER_SYNC_TEST_PROFILE_SYNC_SERVICE_H_
#pragma once

#include <string>

#include "chrome/browser/sync/glue/data_type_manager_impl.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/test/profile_mock.h"
#include "chrome/test/sync/engine/test_id_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

class Profile;
class Task;
class TestProfileSyncService;

ACTION(ReturnNewDataTypeManager) {
  return new browser_sync::DataTypeManagerImpl(arg0, arg1);
}

namespace browser_sync {

// Mocks out the SyncerThread operations (Pause/Resume) since no thread is
// running in these tests, and allows tests to provide a task on construction
// to set up initial nodes to mock out an actual server initial sync
// download.
class SyncBackendHostForProfileSyncTest : public SyncBackendHost {
 public:
  // |synchronous_init| causes initialization to block until the syncapi has
  //     completed setting itself up and called us back.
  SyncBackendHostForProfileSyncTest(
      Profile* profile,
      int num_expected_resumes,
      int num_expected_pauses,
      bool set_initial_sync_ended_on_init,
      bool synchronous_init);

  MOCK_METHOD0(RequestPause, bool());
  MOCK_METHOD0(RequestResume, bool());
  MOCK_METHOD0(RequestNudge, void());

  virtual void ConfigureDataTypes(
      const DataTypeController::TypeMap& data_type_controllers,
      const syncable::ModelTypeSet& types,
      CancelableTask* ready_task);

  // Called when a nudge comes in.
  void SimulateSyncCycleCompletedInitialSyncEnded();

  virtual sync_api::HttpPostProviderFactory* MakeHttpBridgeFactory(
      URLRequestContextGetter* getter);

  virtual void InitCore(const Core::DoInitializeOptions& options);

  static void SetDefaultExpectationsForWorkerCreation(ProfileMock* profile);

  static void SetHistoryServiceExpectations(ProfileMock* profile);

 private:
  bool synchronous_init_;
};

}  // namespace browser_sync

class TestProfileSyncService : public ProfileSyncService {
 public:
  // |initial_condition_setup_task| can be used to populate nodes
  // before the OnBackendInitialized callback fires.
  TestProfileSyncService(ProfileSyncFactory* factory,
                         Profile* profile,
                         const std::string& test_user,
                         bool synchronous_backend_initialization,
                         Task* initial_condition_setup_task);

  virtual ~TestProfileSyncService();

  void SetInitialSyncEndedForEnabledTypes();

  virtual void OnBackendInitialized();

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  void set_num_expected_resumes(int times);
  void set_num_expected_pauses(int num);

  // If this is called, configuring data types will require a syncer
  // nudge.
  void dont_set_initial_sync_ended_on_init();
  void set_synchronous_sync_configuration();

  browser_sync::TestIdFactory* id_factory();

  // Override of ProfileSyncService::GetBackendForTest() with a more
  // specific return type (since C++ supports covariant return types)
  // that is made public.
  virtual browser_sync::SyncBackendHostForProfileSyncTest*
      GetBackendForTest();

 protected:
  virtual void CreateBackend();

 private:
  // When testing under ChromiumOS, this method must not return an empty
  // value value in order for the profile sync service to start.
  virtual std::string GetLsidForAuthBootstraping();

  browser_sync::TestIdFactory id_factory_;

  bool synchronous_backend_initialization_;

  // Set to true when a mock data type manager is being used and the configure
  // step is performed synchronously.
  bool synchronous_sync_configuration_;
  bool set_expect_resume_expectations_;
  int num_expected_resumes_;
  int num_expected_pauses_;

  Task* initial_condition_setup_task_;
  bool set_initial_sync_ended_on_init_;
};



#endif  // CHROME_BROWSER_SYNC_TEST_PROFILE_SYNC_SERVICE_H_
