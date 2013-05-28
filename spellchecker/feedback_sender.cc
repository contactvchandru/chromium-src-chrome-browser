// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/feedback_sender.h"

#include <algorithm>
#include <iterator>

#include "base/command_line.h"
#include "base/hash.h"
#include "base/json/json_writer.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/browser/spellchecker/word_trimmer.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/spellcheck_common.h"
#include "chrome/common/spellcheck_marker.h"
#include "chrome/common/spellcheck_messages.h"
#include "content/public/browser/render_process_host.h"
#include "google_apis/google_api_keys.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"

namespace spellcheck {

namespace {

// The default URL where feedback data is sent.
const char kFeedbackServiceURL[] = "https://www.googleapis.com/rpc";

// Returns a hash of |session_start|, the current timestamp, and
// |suggestion_index|.
uint32 BuildHash(const base::Time& session_start, size_t suggestion_index) {
  std::stringstream hash_data;
  hash_data << session_start.ToTimeT()
            << base::Time::Now().ToTimeT()
            << suggestion_index;
  return base::Hash(hash_data.str());
}

// Returns a pending feedback data structure for the spellcheck |result| and
// |text|.
Misspelling BuildFeedback(const SpellCheckResult& result,
                          const string16& text) {
  size_t start = result.location;
  string16 context = TrimWords(&start,
                               result.length,
                               text,
                               chrome::spellcheck_common::kContextWordCount);
  return Misspelling(context,
                     start,
                     result.length,
                     std::vector<string16>(1, result.replacement),
                     result.hash);
}

// Builds suggestion info from |suggestions|. The caller owns the result.
base::ListValue* BuildSuggestionInfo(
    const std::vector<Misspelling>& suggestions,
    bool is_first_feedback_batch) {
  base::ListValue* list = new base::ListValue;
  for (std::vector<Misspelling>::const_iterator it = suggestions.begin();
       it != suggestions.end();
       ++it) {
    base::DictionaryValue* suggestion = it->Serialize();
    suggestion->SetBoolean("isFirstInSession", is_first_feedback_batch);
    suggestion->SetBoolean("isAutoCorrection", false);
    list->Append(suggestion);
  }
  return list;
}

// Builds feedback parameters from |suggestion_info|, |language|, and |country|.
// Takes ownership of |suggestion_list|. The caller owns the result.
base::DictionaryValue* BuildParams(base::ListValue* suggestion_info,
                                   const std::string& language,
                                   const std::string& country) {
  base::DictionaryValue* params = new base::DictionaryValue;
  params->Set("suggestionInfo", suggestion_info);
  params->SetString("key", google_apis::GetAPIKey());
  params->SetString("language", language);
  params->SetString("originCountry", country);
  params->SetString("clientName", "Chrome");
  return params;
}

// Builds feedback data from |params|. Takes ownership of |params|. The caller
// owns the result.
base::Value* BuildFeedbackValue(base::DictionaryValue* params) {
  base::DictionaryValue* result = new base::DictionaryValue;
  result->Set("params", params);
  result->SetString("method", "spelling.feedback");
  result->SetString("apiVersion", "v2");
  return result;
}

}  // namespace

FeedbackSender::FeedbackSender(net::URLRequestContextGetter* request_context,
                               const std::string& language,
                               const std::string& country)
    : request_context_(request_context),
      language_(language),
      country_(country),
      misspelling_counter_(0),
      session_start_(base::Time::Now()),
      feedback_service_url_(kFeedbackServiceURL) {
  // The command-line switch is for testing and temporary.
  // TODO(rouslan): Remove the command-line switch when testing is complete by
  // August 2013.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSpellingServiceFeedbackUrl)) {
    feedback_service_url_ =
        GURL(CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kSpellingServiceFeedbackUrl));
  }

  timer_.Start(FROM_HERE,
               base::TimeDelta::FromSeconds(
                   chrome::spellcheck_common::kFeedbackIntervalSeconds),
               this,
               &FeedbackSender::RequestDocumentMarkers);
}

FeedbackSender::~FeedbackSender() {
}

void FeedbackSender::SelectedSuggestion(uint32 hash, int suggestion_index) {
  Misspelling* misspelling = feedback_.GetMisspelling(hash);
  if (!misspelling)
    return;
  misspelling->action.type = SpellcheckAction::TYPE_SELECT;
  misspelling->action.index = suggestion_index;
  misspelling->timestamp = base::Time::Now();
}

void FeedbackSender::AddedToDictionary(uint32 hash) {
  Misspelling* misspelling = feedback_.GetMisspelling(hash);
  if (!misspelling)
    return;
  misspelling->action.type = SpellcheckAction::TYPE_ADD_TO_DICT;
  misspelling->timestamp = base::Time::Now();
}

void FeedbackSender::IgnoredSuggestions(uint32 hash) {
  Misspelling* misspelling = feedback_.GetMisspelling(hash);
  if (!misspelling)
    return;
  misspelling->action.type = SpellcheckAction::TYPE_PENDING_IGNORE;
  misspelling->timestamp = base::Time::Now();
}

void FeedbackSender::ManuallyCorrected(uint32 hash,
                                       const string16& correction) {
  Misspelling* misspelling = feedback_.GetMisspelling(hash);
  if (!misspelling)
    return;
  misspelling->action.type = SpellcheckAction::TYPE_MANUALLY_CORRECTED;
  misspelling->action.value = correction;
  misspelling->timestamp = base::Time::Now();
}

void FeedbackSender::OnReceiveDocumentMarkers(
    int renderer_process_id,
    const std::vector<uint32>& markers) {
  if ((base::Time::Now() - session_start_).InHours() >=
      chrome::spellcheck_common::kSessionHours) {
    FlushFeedback();
    return;
  }

  if (!feedback_.RendererHasMisspellings(renderer_process_id))
    return;

  feedback_.FinalizeRemovedMisspellings(renderer_process_id, markers);
  SendFeedback(feedback_.GetMisspellingsInRenderer(renderer_process_id),
               !renderers_sent_feedback_.count(renderer_process_id));
  renderers_sent_feedback_.insert(renderer_process_id);
  feedback_.EraseFinalizedMisspellings(renderer_process_id);
}

void FeedbackSender::OnSpellcheckResults(
    std::vector<SpellCheckResult>* results,
    int renderer_process_id,
    const string16& text,
    const std::vector<SpellCheckMarker>& markers) {
  // Generate a map of marker offsets to marker hashes. This map helps to
  // efficiently lookup feedback data based on the position of the misspelling
  // in text
  typedef std::map<size_t, uint32> MarkerMap;
  MarkerMap marker_map;
  for (size_t i = 0; i < markers.size(); ++i)
    marker_map[markers[i].offset] = markers[i].hash;

  for (std::vector<SpellCheckResult>::iterator result_iter = results->begin();
       result_iter != results->end();
       ++result_iter) {
    MarkerMap::iterator marker_iter = marker_map.find(result_iter->location);
    if (marker_iter != marker_map.end() &&
        feedback_.HasMisspelling(marker_iter->second)) {
      // If the renderer already has a marker for this spellcheck result, then
      // set the hash of the spellcheck result to be the same as the marker.
      result_iter->hash = marker_iter->second;
    } else {
      // If the renderer does not yet have a marker for this spellcheck result,
      // then generate a new hash for the spellcheck result.
      result_iter->hash = BuildHash(session_start_, ++misspelling_counter_);
    }
    // Save the feedback data for the spellcheck result.
    feedback_.AddMisspelling(renderer_process_id,
                             BuildFeedback(*result_iter, text));
  }
}

void FeedbackSender::OnLanguageCountryChange(const std::string& language,
                                             const std::string& country) {
  FlushFeedback();
  language_ = language;
  country_ = country;
}

void FeedbackSender::OnURLFetchComplete(const net::URLFetcher* source) {
  for (ScopedVector<net::URLFetcher>::iterator it = senders_.begin();
       it != senders_.end();
       ++it) {
    if (*it == source) {
      senders_.erase(it);
      break;
    }
  }
}

void FeedbackSender::RequestDocumentMarkers() {
  // Request document markers from all the renderers that are still alive.
  std::vector<int> alive_renderers;
  for (content::RenderProcessHost::iterator it(
           content::RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd();
       it.Advance()) {
    alive_renderers.push_back(it.GetCurrentValue()->GetID());
    it.GetCurrentValue()->Send(new SpellCheckMsg_RequestDocumentMarkers());
  }

  // Asynchronously send out the feedback for all the renderers that are no
  // longer alive.
  std::vector<int> known_renderers = feedback_.GetRendersWithMisspellings();
  std::sort(alive_renderers.begin(), alive_renderers.end());
  std::sort(known_renderers.begin(), known_renderers.end());
  std::vector<int> dead_renderers;
  std::set_difference(known_renderers.begin(),
                      known_renderers.end(),
                      alive_renderers.begin(),
                      alive_renderers.end(),
                      std::back_inserter(dead_renderers));
  for (std::vector<int>::const_iterator it = dead_renderers.begin();
       it != dead_renderers.end();
       ++it) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&FeedbackSender::OnReceiveDocumentMarkers,
                   AsWeakPtr(),
                   *it,
                   std::vector<uint32>()));
  }
}

void FeedbackSender::FlushFeedback() {
  if (feedback_.Empty())
    return;
  feedback_.FinalizeAllMisspellings();
  SendFeedback(feedback_.GetAllMisspellings(),
               renderers_sent_feedback_.empty());
  feedback_.Clear();
  renderers_sent_feedback_.clear();
  session_start_ = base::Time::Now();
  timer_.Reset();
}

void FeedbackSender::SendFeedback(const std::vector<Misspelling>& feedback_data,
                                  bool is_first_feedback_batch) {
  scoped_ptr<base::Value> feedback_value(BuildFeedbackValue(
      BuildParams(BuildSuggestionInfo(feedback_data, is_first_feedback_batch),
                  language_,
                  country_)));
  std::string feedback;
  base::JSONWriter::Write(feedback_value.get(), &feedback);

  // The tests use this identifier to mock the URL fetcher.
  static const int kUrlFetcherId = 0;
  net::URLFetcher* sender = net::URLFetcher::Create(
      kUrlFetcherId, feedback_service_url_, net::URLFetcher::POST, this);
  sender->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                       net::LOAD_DO_NOT_SAVE_COOKIES);
  sender->SetUploadData("application/json", feedback);
  senders_.push_back(sender);

  // Request context is NULL in testing.
  if (request_context_.get()) {
    sender->SetRequestContext(request_context_.get());
    sender->Start();
  }
}

}  // namespace spellcheck
