// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file.h"

#include <io.h>
#include <stdint.h>

#include "base/logging.h"
#include "base/win/win_util.h"

#include <windows.h>

namespace base {

// Make sure our Whence mappings match the system headers.
static_assert(File::FROM_BEGIN == FILE_BEGIN &&
                  File::FROM_CURRENT == FILE_CURRENT &&
                  File::FROM_END == FILE_END,
              "whence mapping must match the system headers");

bool File::IsValid() const {
  return file_.IsValid();
}

PlatformFile File::GetPlatformFile() const {
  return file_.Get();
}

PlatformFile File::TakePlatformFile() {
  return file_.Take();
}

void File::Close() {
  if (!file_.IsValid())
    return;

  file_.Close();
}

int64_t File::Seek(Whence whence, int64_t offset) {
  DCHECK(IsValid());

  LARGE_INTEGER distance, res;
  distance.QuadPart = offset;
  DWORD move_method = static_cast<DWORD>(whence);
  if (!SetFilePointerEx(file_.Get(), distance, &res, move_method))
    return -1;
  return res.QuadPart;
}

int File::Read(int64_t offset, char* data, int size) {
  DCHECK(IsValid());
  if (size < 0)
    return -1;

  LARGE_INTEGER offset_li;
  offset_li.QuadPart = offset;

  OVERLAPPED overlapped = {};
  overlapped.Offset = offset_li.LowPart;
  overlapped.OffsetHigh = offset_li.HighPart;

  DWORD bytes_read;
  if (::ReadFile(file_.Get(), data, size, &bytes_read, &overlapped))
    return bytes_read;
  if (ERROR_HANDLE_EOF == GetLastError())
    return 0;

  return -1;
}

int File::ReadAtCurrentPos(char* data, int size) {
  DCHECK(IsValid());
  if (size < 0)
    return -1;

  DWORD bytes_read;
  if (::ReadFile(file_.Get(), data, size, &bytes_read, NULL))
    return bytes_read;
  if (ERROR_HANDLE_EOF == GetLastError())
    return 0;

  return -1;
}

int File::ReadNoBestEffort(int64_t offset, char* data, int size) {
  // TODO(dbeam): trace this separately?
  return Read(offset, data, size);
}

int File::ReadAtCurrentPosNoBestEffort(char* data, int size) {
  // TODO(dbeam): trace this separately?
  return ReadAtCurrentPos(data, size);
}

int File::Write(int64_t offset, const char* data, int size) {
  DCHECK(IsValid());

  LARGE_INTEGER offset_li;
  offset_li.QuadPart = offset;

  OVERLAPPED overlapped = {};
  overlapped.Offset = offset_li.LowPart;
  overlapped.OffsetHigh = offset_li.HighPart;

  DWORD bytes_written;
  if (::WriteFile(file_.Get(), data, size, &bytes_written, &overlapped))
    return bytes_written;

  return -1;
}

int File::WriteAtCurrentPos(const char* data, int size) {
  DCHECK(IsValid());
  if (size < 0)
    return -1;

  DWORD bytes_written;
  if (::WriteFile(file_.Get(), data, size, &bytes_written, NULL))
    return bytes_written;

  return -1;
}

int File::WriteAtCurrentPosNoBestEffort(const char* data, int size) {
  return WriteAtCurrentPos(data, size);
}

int64_t File::GetLength() {
  DCHECK(IsValid());

  LARGE_INTEGER size;
  if (!::GetFileSizeEx(file_.Get(), &size))
    return -1;

  return static_cast<int64_t>(size.QuadPart);
}

bool File::SetLength(int64_t length) {
  DCHECK(IsValid());

  // Get the current file pointer.
  LARGE_INTEGER file_pointer;
  LARGE_INTEGER zero;
  zero.QuadPart = 0;
  if (!::SetFilePointerEx(file_.Get(), zero, &file_pointer, FILE_CURRENT))
    return false;

  LARGE_INTEGER length_li;
  length_li.QuadPart = length;
  // If length > file size, SetFilePointerEx() should extend the file
  // with zeroes on all Windows standard file systems (NTFS, FATxx).
  if (!::SetFilePointerEx(file_.Get(), length_li, NULL, FILE_BEGIN))
    return false;

  // Set the new file length and move the file pointer to its old position.
  // This is consistent with ftruncate()'s behavior, even when the file
  // pointer points to a location beyond the end of the file.
  // TODO(rvargas): Emulating ftruncate details seem suspicious and it is not
  // promised by the interface (nor was promised by PlatformFile). See if this
  // implementation detail can be removed.
  return ((::SetEndOfFile(file_.Get()) != FALSE) &&
          (::SetFilePointerEx(file_.Get(), file_pointer, NULL, FILE_BEGIN) !=
           FALSE));
}

bool File::GetInfo(Info* info) {
  DCHECK(IsValid());

  BY_HANDLE_FILE_INFORMATION file_info;
  if (!GetFileInformationByHandle(file_.Get(), &file_info))
    return false;

  LARGE_INTEGER size;
  size.HighPart = file_info.nFileSizeHigh;
  size.LowPart = file_info.nFileSizeLow;
  info->size = size.QuadPart;
  info->is_directory =
      (file_info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
  info->is_symbolic_link = false;  // Windows doesn't have symbolic links.
  info->last_modified =
      *reinterpret_cast<uint64_t*>(&file_info.ftLastWriteTime);
  info->last_accessed =
      *reinterpret_cast<uint64_t*>(&file_info.ftLastAccessTime);
  info->creation_time = *reinterpret_cast<uint64_t*>(&file_info.ftCreationTime);
  return true;
}

File::Error File::Lock() {
  DCHECK(IsValid());

  BOOL result = LockFile(file_.Get(), 0, 0, MAXDWORD, MAXDWORD);
  if (!result)
    return GetLastFileError();
  return FILE_OK;
}

File::Error File::Unlock() {
  DCHECK(IsValid());

  BOOL result = UnlockFile(file_.Get(), 0, 0, MAXDWORD, MAXDWORD);
  if (!result)
    return GetLastFileError();
  return FILE_OK;
}

File File::Duplicate() const {
  if (!IsValid())
    return File();

  HANDLE other_handle = nullptr;

  if (!::DuplicateHandle(GetCurrentProcess(),  // hSourceProcessHandle
                         GetPlatformFile(),
                         GetCurrentProcess(),  // hTargetProcessHandle
                         &other_handle,
                         0,      // dwDesiredAccess ignored due to SAME_ACCESS
                         FALSE,  // !bInheritHandle
                         DUPLICATE_SAME_ACCESS)) {
    return File(GetLastFileError());
  }

  File other(other_handle);
  return other;
}

// Static.
File::Error File::OSErrorToFileError(DWORD last_error) {
  switch (last_error) {
    case ERROR_SHARING_VIOLATION:
      return FILE_ERROR_IN_USE;
    case ERROR_ALREADY_EXISTS:
    case ERROR_FILE_EXISTS:
      return FILE_ERROR_EXISTS;
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
      return FILE_ERROR_NOT_FOUND;
    case ERROR_ACCESS_DENIED:
      return FILE_ERROR_ACCESS_DENIED;
    case ERROR_TOO_MANY_OPEN_FILES:
      return FILE_ERROR_TOO_MANY_OPENED;
    case ERROR_OUTOFMEMORY:
    case ERROR_NOT_ENOUGH_MEMORY:
      return FILE_ERROR_NO_MEMORY;
    case ERROR_HANDLE_DISK_FULL:
    case ERROR_DISK_FULL:
#ifndef __MINGW32__
    case ERROR_DISK_RESOURCES_EXHAUSTED:
#endif
      return FILE_ERROR_NO_SPACE;
    case ERROR_USER_MAPPED_FILE:
      return FILE_ERROR_INVALID_OPERATION;
    case ERROR_NOT_READY:
    case ERROR_SECTOR_NOT_FOUND:
    case ERROR_DEV_NOT_EXIST:
    case ERROR_IO_DEVICE:
    case ERROR_FILE_CORRUPT:
    case ERROR_DISK_CORRUPT:
      return FILE_ERROR_IO;
    default:
      // This function should only be called for errors.
      DCHECK_NE(static_cast<DWORD>(ERROR_SUCCESS), last_error);
      return FILE_ERROR_FAILED;
  }
}

void File::DoInitialize(const FilePath& path, uint32_t flags) {
  DCHECK(!IsValid());

  DWORD disposition = 0;

  if (flags & FLAG_OPEN)
    disposition = OPEN_EXISTING;

  if (flags & FLAG_CREATE_ALWAYS) {
    DCHECK(!disposition);
    DCHECK(flags & FLAG_WRITE);
    disposition = CREATE_ALWAYS;
  }

  if (!disposition) {
    ::SetLastError(ERROR_INVALID_PARAMETER);
    error_details_ = FILE_ERROR_FAILED;
    NOTREACHED();
    return;
  }

  DWORD access = 0;
  if (flags & FLAG_WRITE)
    access = GENERIC_WRITE;
  if (flags & FLAG_READ)
    access |= GENERIC_READ;

  DWORD sharing = FILE_SHARE_READ | FILE_SHARE_WRITE;
  DWORD create_flags = 0;
  file_.Set(CreateFile(ToWCharT(&path.value()), access, sharing, NULL,
                       disposition, create_flags, NULL));

  if (file_.IsValid()) {
    error_details_ = FILE_OK;
    if (flags & FLAG_CREATE_ALWAYS)
      created_ = true;
  } else {
    error_details_ = GetLastFileError();
  }
}

bool File::Flush() {
  DCHECK(IsValid());
  return ::FlushFileBuffers(file_.Get()) != FALSE;
}

void File::SetPlatformFile(PlatformFile file) {
  file_.Set(file);
}

// static
File::Error File::GetLastFileError() {
  return File::OSErrorToFileError(GetLastError());
}

}  // namespace base
