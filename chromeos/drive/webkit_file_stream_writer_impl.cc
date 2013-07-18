// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/webkit_file_stream_writer_impl.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/fileapi_worker.h"
#include "chrome/browser/google_apis/task_util.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "webkit/browser/fileapi/local_file_stream_writer.h"
#include "webkit/browser/fileapi/remote_file_system_proxy.h"

using content::BrowserThread;

namespace drive {
namespace internal {
namespace {

// Creates a writable snapshot file of the |drive_path|.
void CreateWritableSnapshotFile(
    const WebkitFileStreamWriterImpl::FileSystemGetter& file_system_getter,
    const base::FilePath& drive_path,
    const fileapi_internal::CreateWritableSnapshotFileCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          &fileapi_internal::RunFileSystemCallback,
          file_system_getter,
          base::Bind(&fileapi_internal::CreateWritableSnapshotFile,
                     drive_path, google_apis::CreateRelayCallback(callback)),
          google_apis::CreateRelayCallback(base::Bind(
              callback, base::PLATFORM_FILE_ERROR_FAILED, base::FilePath()))));
}

// Closes the writable snapshot file opened by CreateWritableSnapshotFile.
// TODO(hidehiko): Get rid of this function. crbug.com/259184.
void CloseFile(
    const WebkitFileStreamWriterImpl::FileSystemGetter& file_system_getter,
    const base::FilePath& drive_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&fileapi_internal::RunFileSystemCallback,
                 file_system_getter,
                 base::Bind(&fileapi_internal::CloseFile, drive_path),
                 base::Closure()));
}

}  // namespace

WebkitFileStreamWriterImpl::WebkitFileStreamWriterImpl(
    const FileSystemGetter& file_system_getter,
    base::TaskRunner* file_task_runner,
    const base::FilePath& file_path,
    int64 offset)
    : file_system_getter_(file_system_getter),
      file_task_runner_(file_task_runner),
      file_path_(file_path),
      offset_(offset),
      weak_ptr_factory_(this) {
}

WebkitFileStreamWriterImpl::~WebkitFileStreamWriterImpl() {
  if (local_file_writer_) {
    // If the file is opened, close it at destructor.
    // It is necessary to close the local file in advance.
    local_file_writer_.reset();
    CloseFile(file_system_getter_, file_path_);
  }
}

int WebkitFileStreamWriterImpl::Write(net::IOBuffer* buf,
                                      int buf_len,
                                      const net::CompletionCallback& callback) {
  DCHECK(pending_write_callback_.is_null());
  DCHECK(pending_cancel_callback_.is_null());
  DCHECK(!callback.is_null());

  // If the local file is already available, just delegate to it.
  if (local_file_writer_)
    return local_file_writer_->Write(buf, buf_len, callback);

  // The local file is not yet ready. Create the writable snapshot.
  if (file_path_.empty())
    return net::ERR_FILE_NOT_FOUND;

  pending_write_callback_ = callback;
  CreateWritableSnapshotFile(
      file_system_getter_, file_path_,
      base::Bind(
          &WebkitFileStreamWriterImpl::WriteAfterCreateWritableSnapshotFile,
          weak_ptr_factory_.GetWeakPtr(), make_scoped_refptr(buf), buf_len));
  return net::ERR_IO_PENDING;
}

int WebkitFileStreamWriterImpl::Cancel(
    const net::CompletionCallback& callback) {
  DCHECK(pending_cancel_callback_.is_null());
  DCHECK(!callback.is_null());

  // If LocalFileWriter is already created, just delegate the cancel to it.
  if (local_file_writer_)
    return local_file_writer_->Cancel(callback);

  // If file open operation is in-flight, wait for its completion and cancel
  // further write operation in WriteAfterCreateWritableSnapshotFile.
  if (!pending_write_callback_.is_null()) {
    // Dismiss pending write callback immediately.
    pending_write_callback_.Reset();
    pending_cancel_callback_ = callback;
    return net::ERR_IO_PENDING;
  }

  // Write() is not called yet.
  return net::ERR_UNEXPECTED;
}

int WebkitFileStreamWriterImpl::Flush(const net::CompletionCallback& callback) {
  DCHECK(pending_cancel_callback_.is_null());
  DCHECK(!callback.is_null());

  // If LocalFileWriter is already created, just delegate to it.
  if (local_file_writer_)
    return local_file_writer_->Flush(callback);

  // There shouldn't be in-flight Write operation.
  DCHECK(pending_write_callback_.is_null());

  // Here is the case Flush() is called before any Write() invocation.
  // Do nothing.
  // Synchronization to the remote server is not done until the file is closed.
  return net::OK;
}

void WebkitFileStreamWriterImpl::WriteAfterCreateWritableSnapshotFile(
    net::IOBuffer* buf,
    int buf_len,
    base::PlatformFileError open_result,
    const base::FilePath& local_path) {
  DCHECK(!local_file_writer_);

  if (!pending_cancel_callback_.is_null()) {
    DCHECK(pending_write_callback_.is_null());
    // Cancel() is called during the creation of the snapshot file.
    // Don't write to the file.
    if (open_result == base::PLATFORM_FILE_OK) {
      // Here the file is internally created. To revert the operation, close
      // the file.
      DCHECK(!local_path.empty());
      CloseFile(file_system_getter_, file_path_);
    }

    base::ResetAndReturn(&pending_cancel_callback_).Run(net::OK);
    return;
  }

  DCHECK(!pending_write_callback_.is_null());

  const net::CompletionCallback callback =
      base::ResetAndReturn(&pending_write_callback_);
  if (open_result != base::PLATFORM_FILE_OK) {
    callback.Run(net::PlatformFileErrorToNetError(open_result));
    return;
  }

  local_file_writer_.reset(new fileapi::LocalFileStreamWriter(
      file_task_runner_.get(), local_path, offset_));
  int result = local_file_writer_->Write(buf, buf_len, callback);
  if (result != net::ERR_IO_PENDING)
    callback.Run(result);
}

}  // namespace internal
}  // namespace drive
