/* Copyright  (C) 2018-2020 The RetroArch team
*
* ---------------------------------------------------------------------------------------
* The following license statement only applies to this file (vfs_implementation_uwp.cpp).
* ---------------------------------------------------------------------------------------
*
* Permission is hereby granted, free of charge,
* to any person obtaining a copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation the rights to
* use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
* and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <retro_environment.h>

#include <ppl.h>
#include <ppltasks.h>
#include <stdio.h>
#include <wrl.h>
#include <wrl/implements.h>
#include <windows.storage.streams.h>
#include <robuffer.h>
#include <collection.h>
#include <functional>
#include <fileapifromapp.h>

using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Storage;
using namespace Windows::Storage::Streams;
using namespace Windows::Storage::FileProperties;

#ifdef RARCH_INTERNAL
#ifndef VFS_FRONTEND
#define VFS_FRONTEND
#endif
#endif

#include <vfs/vfs.h>
#include <vfs/vfs_implementation.h>
#include <libretro.h>
#include <encodings/utf.h>
#include <retro_miscellaneous.h>
#include <file/file_path.h>
#include <retro_assert.h>
#include <string/stdstring.h>
#include <retro_environment.h>
#include <uwp/uwp_async.h>
#include <uwp/uwp_file_handle_access.h>

namespace
{
   void windowsize_path(wchar_t* path)
   {
      /* UWP deals with paths containing / instead of 
       * \ way worse than normal Windows */
      /* and RetroArch may sometimes mix them 
       * (e.g. on archive extraction) */
      if (!path)
         return;

      while (*path)
      {
         if (*path == '/')
            *path = '\\';
         ++path;
      }
   }
}

namespace
{
   /* Damn you, UWP, why no functions for that either */
   template<typename T>
   concurrency::task<T^> GetItemFromPathAsync(Platform::String^ path)
   {
      static_assert(false, "StorageFile and StorageFolder only");
   }
   template<>
   concurrency::task<StorageFile^> GetItemFromPathAsync(Platform::String^ path)
   {
      return concurrency::create_task(StorageFile::GetFileFromPathAsync(path));
   }
   template<>
   concurrency::task<StorageFolder^> GetItemFromPathAsync(Platform::String^ path)
   {
      return concurrency::create_task(StorageFolder::GetFolderFromPathAsync(path));
   }

   template<typename T>
   concurrency::task<T^> GetItemInFolderFromPathAsync(StorageFolder^ folder, Platform::String^ path)
   {
      static_assert(false, "StorageFile and StorageFolder only");
   }
   template<>
   concurrency::task<StorageFile^> GetItemInFolderFromPathAsync(StorageFolder^ folder, Platform::String^ path)
   {
      if (path->IsEmpty())
         retro_assert(false); /* Attempt to read a folder as a file - this really should have been caught earlier */
      return concurrency::create_task(folder->GetFileAsync(path));
   }
   template<>
   concurrency::task<StorageFolder^> GetItemInFolderFromPathAsync(StorageFolder^ folder, Platform::String^ path)
   {
      if (path->IsEmpty())
         return concurrency::create_task(concurrency::create_async([folder]() { return folder; }));
      return concurrency::create_task(folder->GetFolderAsync(path));
   }
}

namespace
{
   /* A list of all StorageFolder objects returned using from the file picker */
   Platform::Collections::Vector<StorageFolder^> accessible_directories;

   concurrency::task<Platform::String^> TriggerPickerAddDialog()
   {
      auto folderPicker = ref new Windows::Storage::Pickers::FolderPicker();
      folderPicker->SuggestedStartLocation = Windows::Storage::Pickers::PickerLocationId::Desktop;
      folderPicker->FileTypeFilter->Append("*");

      return concurrency::create_task(folderPicker->PickSingleFolderAsync()).then([](StorageFolder^ folder) {
         if (folder == nullptr)
            throw ref new Platform::Exception(E_ABORT, L"Operation cancelled by user");

         /* TODO: check for duplicates */
         accessible_directories.Append(folder);
         return folder->Path;
      });
   }

   template<typename T>
   concurrency::task<T^> LocateStorageItem(Platform::String^ path)
   {
      /* Look for a matching directory we can use */
      for each (StorageFolder^ folder in accessible_directories)
      {
         std::wstring file_path;
         std::wstring folder_path = folder->Path->Data();
         size_t folder_path_size  = folder_path.size();
         /* Could be C:\ or C:\Users\somebody - remove the trailing slash to unify them */
         if (folder_path[folder_path_size - 1] == '\\')
            folder_path.erase(folder_path_size - 1);

         file_path = path->Data();

         if (file_path.find(folder_path) == 0)
         {
            /* Found a match */
            file_path = file_path.length() > folder_path.length() 
               ? file_path.substr(folder_path.length() + 1) 
               : L"";
            return concurrency::create_task(GetItemInFolderFromPathAsync<T>(folder, ref new Platform::String(file_path.data())));
         }
      }

      /* No matches - try accessing directly, and fallback to user prompt */
      return concurrency::create_task(GetItemFromPathAsync<T>(path)).then([&](concurrency::task<T^> item) {
         try
         {
            T^ storageItem = item.get();
            return concurrency::create_task(concurrency::create_async([storageItem]() { return storageItem; }));
         }
         catch (Platform::AccessDeniedException^ e)
         {
            Windows::UI::Popups::MessageDialog^ dialog =
               ref new Windows::UI::Popups::MessageDialog("Path \"" + path + "\" is not currently accessible. Please open any containing directory to access it.");
            dialog->Commands->Append(ref new Windows::UI::Popups::UICommand("Open file picker"));
            dialog->Commands->Append(ref new Windows::UI::Popups::UICommand("Cancel"));
            return concurrency::create_task(dialog->ShowAsync()).then([path](Windows::UI::Popups::IUICommand^ cmd) {
               if (cmd->Label == "Open file picker")
               {
                  return TriggerPickerAddDialog().then([path](Platform::String^ added_path) {
                     /* Retry */
                     return LocateStorageItem<T>(path);
                  });
               }
               else
               {
                  throw ref new Platform::Exception(E_ABORT, L"Operation cancelled by user");
               }
            });
         }
      });
   }

   IStorageItem^ LocateStorageFileOrFolder(Platform::String^ path)
   {
      if (!path || path->IsEmpty())
         return nullptr;

      if (*(path->End() - 1) == '\\')
      {
         /* Ends with a slash, so it's definitely a directory */
         return RunAsyncAndCatchErrors<StorageFolder^>([&]() {
            return concurrency::create_task(LocateStorageItem<StorageFolder>(path));
         }, nullptr);
      }
      else
      {
         /* No final slash - probably a file (since RetroArch usually slash-terminates dirs), but there is still a chance it's a directory */
         IStorageItem^ item;
         item = RunAsyncAndCatchErrors<StorageFile^>([&]() {
            return concurrency::create_task(LocateStorageItem<StorageFile>(path));
         }, nullptr);
         if (!item)
         {
            item = RunAsyncAndCatchErrors<StorageFolder^>([&]() {
               return concurrency::create_task(LocateStorageItem<StorageFolder>(path));
            }, nullptr);
         }
         return item;
      }
   }
}


/* This is some pure magic and I have absolutely no idea how it works */
/* Wraps a raw buffer into a WinRT object */
/* https://stackoverflow.com/questions/10520335/how-to-wrap-a-char-buffer-in-a-winrt-ibuffer-in-c */
class NativeBuffer :
   public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::WinRtClassicComMix>,
   ABI::Windows::Storage::Streams::IBuffer,
   Windows::Storage::Streams::IBufferByteAccess>
{
public:
   virtual ~NativeBuffer()
   {
   }

   HRESULT __stdcall RuntimeClassInitialize(
         byte *buffer, uint32_t capacity, uint32_t length)
   {
      m_buffer = buffer;
      m_capacity = capacity;
      m_length = length;
      return S_OK;
   }

   HRESULT __stdcall Buffer(byte **value)
   {
      if (m_buffer == nullptr)
         return E_INVALIDARG;
      *value = m_buffer;
      return S_OK;
   }

   HRESULT __stdcall get_Capacity(uint32_t *value)
   {
      *value = m_capacity;
      return S_OK;
   }

   HRESULT __stdcall get_Length(uint32_t *value)
   {
      *value = m_length;
      return S_OK;
   }

   HRESULT __stdcall put_Length(uint32_t value)
   {
      if (value > m_capacity)
         return E_INVALIDARG;
      m_length = value;
      return S_OK;
   }

private:
   byte *m_buffer;
   uint32_t m_capacity;
   uint32_t m_length;
};

IBuffer^ CreateNativeBuffer(void* buf, uint32_t capacity, uint32_t length)
{
   Microsoft::WRL::ComPtr<NativeBuffer> nativeBuffer;
   Microsoft::WRL::Details::MakeAndInitialize<NativeBuffer>(&nativeBuffer, (byte *)buf, capacity, length);
   auto iinspectable = (IInspectable *)reinterpret_cast<IInspectable *>(nativeBuffer.Get());
   IBuffer ^buffer = reinterpret_cast<IBuffer ^>(iinspectable);
   return buffer;
}

/* Get a Win32 file handle out of IStorageFile */
/* https://stackoverflow.com/questions/42799235/how-can-i-get-a-win32-handle-for-a-storagefile-or-storagefolder-in-uwp */
HRESULT GetHandleFromStorageFile(Windows::Storage::StorageFile^ file, HANDLE* handle, HANDLE_ACCESS_OPTIONS accessMode)
{
   Microsoft::WRL::ComPtr<IUnknown> abiPointer(reinterpret_cast<IUnknown*>(file));
   Microsoft::WRL::ComPtr<IStorageItemHandleAccess> handleAccess;
   if (SUCCEEDED(abiPointer.As(&handleAccess)))
   {
      HANDLE hFile = INVALID_HANDLE_VALUE;

      if (SUCCEEDED(handleAccess->Create(accessMode,
                  HANDLE_SHARING_OPTIONS::HSO_SHARE_READ,
                  HANDLE_OPTIONS::HO_NONE,
                  nullptr,
                  &hFile)))
      {
         *handle = hFile;
         return S_OK;
      }
   }

   return E_FAIL;
}

#ifdef VFS_FRONTEND
struct retro_vfs_file_handle
#else
struct libretro_vfs_implementation_file
#endif
{
   IRandomAccessStream^ fp;
   IBuffer^ bufferp;
   HANDLE file_handle;
   char* buffer;
   char* orig_path;
   size_t buffer_size;
   int buffer_left;
   size_t buffer_fill;
};

libretro_vfs_implementation_file *retro_vfs_file_open_impl(
      const char *path, unsigned mode, unsigned hints)
{
   char dirpath[PATH_MAX_LENGTH];
   char filename[PATH_MAX_LENGTH];
   wchar_t *path_wide;
   wchar_t *dirpath_wide;
   wchar_t *filename_wide;
   Platform::String^ path_str;
   Platform::String^ filename_str;
   Platform::String^ dirpath_str;
   HANDLE file_handle = INVALID_HANDLE_VALUE;
   DWORD desireAccess;
   DWORD creationDisposition;

   if (!path || !*path)
      return NULL;

   /* Something tried to access files from current directory. 
    * This is not allowed on UWP. */
   if (!path_is_absolute(path))
      return NULL;

   /* Trying to open a directory as file?! */
   if (PATH_CHAR_IS_SLASH(path[strlen(path) - 1]))
      return NULL;

   dirpath[0] = filename[0] = '\0';

   path_wide             = utf8_to_utf16_string_alloc(path);
   windowsize_path(path_wide);
   path_str              = ref new Platform::String(path_wide);
   free(path_wide);

   fill_pathname_basedir(dirpath, path, sizeof(dirpath));
   dirpath_wide          = utf8_to_utf16_string_alloc(dirpath);
   windowsize_path(dirpath_wide);
   dirpath_str           = ref new Platform::String(dirpath_wide);
   free(dirpath_wide);

   fill_pathname_base(filename, path, sizeof(filename));
   filename_wide         = utf8_to_utf16_string_alloc(filename);
   filename_str          = ref new Platform::String(filename_wide);
   free(filename_wide);

   retro_assert(!dirpath_str->IsEmpty() && !filename_str->IsEmpty());

   /* Try Win32 first, this should work in AppData */
   if (mode == RETRO_VFS_FILE_ACCESS_READ)
   {
      desireAccess        = GENERIC_READ;
      creationDisposition = OPEN_ALWAYS;
   }
   else
   {
      desireAccess        = GENERIC_WRITE;
      creationDisposition = (mode & RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING) != 0 ?
         OPEN_ALWAYS : CREATE_ALWAYS;
   }

   file_handle = CreateFile2FromAppW(path_str->Data(), desireAccess, FILE_SHARE_READ, creationDisposition, NULL);

   if (file_handle != INVALID_HANDLE_VALUE)
   {
      libretro_vfs_implementation_file* stream = (libretro_vfs_implementation_file*)calloc(1, sizeof(*stream));
      if (!stream)
         return (libretro_vfs_implementation_file*)NULL;

      stream->orig_path = strdup(path);
      stream->fp = nullptr;
      stream->file_handle = file_handle;
      stream->buffer_left = 0;
      stream->buffer_fill = 0;
      return stream;
   }

   /* Fallback to WinRT */
   return RunAsyncAndCatchErrors<libretro_vfs_implementation_file*>([&]() {
      return concurrency::create_task(LocateStorageItem<StorageFolder>(dirpath_str)).then([&](StorageFolder^ dir) {
         if (mode == RETRO_VFS_FILE_ACCESS_READ)
            return dir->GetFileAsync(filename_str);
         else
            return dir->CreateFileAsync(filename_str, (mode & RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING) != 0 ?
               CreationCollisionOption::OpenIfExists : CreationCollisionOption::ReplaceExisting);
      }).then([&](StorageFile ^file) {

         HANDLE_CREATION_OPTIONS creationOptions;
         HANDLE_ACCESS_OPTIONS handleAccess;
         HRESULT hr;

         /* Try to use IStorageItemHandleAccess to get the file handle,
          * with that we can use Win32 APIs for subsequent reads/writes
          */
         if (mode == RETRO_VFS_FILE_ACCESS_READ)
         {
            handleAccess    = HANDLE_ACCESS_OPTIONS::HAO_READ;
            creationOptions = HANDLE_CREATION_OPTIONS::HCO_OPEN_ALWAYS;
         }
         else
         {
            handleAccess    = HANDLE_ACCESS_OPTIONS::HAO_WRITE;
            creationOptions = (mode & RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING) != 0 ?
               HANDLE_CREATION_OPTIONS::HCO_OPEN_ALWAYS : HANDLE_CREATION_OPTIONS::HCO_CREATE_ALWAYS;
            
         }
         hr = GetHandleFromStorageFile(file, &file_handle, handleAccess);

         if (SUCCEEDED(hr))
            /* Success, let's return a null pointer and continue */
            return concurrency::create_task([&]() { return (IRandomAccessStream^) nullptr; });
         else
         {
            /* Failed, open a WinRT buffer of the file */
            FileAccessMode accessMode = (mode == RETRO_VFS_FILE_ACCESS_READ) ?
               FileAccessMode::Read : FileAccessMode::ReadWrite;
            return concurrency::create_task(file->OpenAsync(accessMode));
         }
      }).then([&](IRandomAccessStream^ fpstream) {
         libretro_vfs_implementation_file *stream = (libretro_vfs_implementation_file*)calloc(1, sizeof(*stream));
         if (!stream)
            return (libretro_vfs_implementation_file*)NULL;

         stream->orig_path   = strdup(path);
         stream->fp          = fpstream;
         stream->file_handle = file_handle;
         stream->buffer_left = 0;
         stream->buffer_fill = 0;

         if (fpstream)
         {
            /* We are using WinRT.
             * Preallocate a small buffer for manually buffered I/O,
              * makes short read faster */
            stream->fp->Seek(0);
            int buf_size        = 8 * 1024;
            stream->buffer      = (char*)malloc(buf_size);
            stream->bufferp     = CreateNativeBuffer(stream->buffer, buf_size, 0);
            stream->buffer_size = buf_size;
         }
         else
         {
            /* If we can use Win32 file API, buffering shouldn't be necessary */
            stream->buffer      = NULL;
            stream->bufferp     = nullptr;
            stream->buffer_size = 0;
         }
         return stream;
      });
   }, NULL);
}

int retro_vfs_file_close_impl(libretro_vfs_implementation_file *stream)
{
   if (!stream || (!stream->fp && stream->file_handle == INVALID_HANDLE_VALUE))
      return -1;

   if (stream->file_handle != INVALID_HANDLE_VALUE)
      CloseHandle(stream->file_handle);
   else
   {
      /* Apparently, this is how you close a file in WinRT */
      /* Yes, really */
      stream->fp = nullptr;
      free(stream->buffer);
   }

   return 0;
}

int retro_vfs_file_error_impl(libretro_vfs_implementation_file *stream)
{
   return false; /* TODO */
}

int64_t retro_vfs_file_size_impl(libretro_vfs_implementation_file *stream)
{
   if (!stream || (!stream->fp && stream->file_handle == INVALID_HANDLE_VALUE))
      return 0;

   if (stream->file_handle != INVALID_HANDLE_VALUE)
   {
      LARGE_INTEGER sz;
      if (GetFileSizeEx(stream->file_handle, &sz))
         return sz.QuadPart;
      return 0;
   }

   return stream->fp->Size;
}

int64_t retro_vfs_file_truncate_impl(libretro_vfs_implementation_file *stream, int64_t length)
{
   if (!stream || (!stream->fp && stream->file_handle == INVALID_HANDLE_VALUE))
      return -1;

   if (stream->file_handle != INVALID_HANDLE_VALUE)
   {
      int64_t p = retro_vfs_file_tell_impl(stream);
      retro_vfs_file_seek_impl(stream, length, RETRO_VFS_SEEK_POSITION_START);
      SetEndOfFile(stream->file_handle);

      if (p < length)
         retro_vfs_file_seek_impl(stream, p, RETRO_VFS_SEEK_POSITION_START);
   }
   else
      stream->fp->Size = length;
   
   return 0;
}

int64_t retro_vfs_file_tell_impl(libretro_vfs_implementation_file *stream)
{
   LARGE_INTEGER _offset;
   LARGE_INTEGER out;
   _offset.QuadPart = 0;

   if (!stream || (!stream->fp && stream->file_handle == INVALID_HANDLE_VALUE))
      return -1;

   if (stream->file_handle != INVALID_HANDLE_VALUE)
   {
      SetFilePointerEx(stream->file_handle, _offset, &out, FILE_CURRENT);
      return out.QuadPart;
   }

   return stream->fp->Position - stream->buffer_left;
}

int64_t retro_vfs_file_seek_impl(
      libretro_vfs_implementation_file *stream,
      int64_t offset, int seek_position)
{
   LARGE_INTEGER _offset;
   _offset.QuadPart = offset;

   if (!stream || (!stream->fp && stream->file_handle == INVALID_HANDLE_VALUE))
      return -1;

   switch (seek_position)
   {
      case RETRO_VFS_SEEK_POSITION_START:
         if (stream->file_handle != INVALID_HANDLE_VALUE)
            SetFilePointerEx(stream->file_handle, _offset, NULL, FILE_BEGIN);
         else
            stream->fp->Seek(offset);
         break;

      case RETRO_VFS_SEEK_POSITION_CURRENT:
         if (stream->file_handle != INVALID_HANDLE_VALUE)
            SetFilePointerEx(stream->file_handle, _offset, NULL, FILE_CURRENT);
         else
            stream->fp->Seek(retro_vfs_file_tell_impl(stream) + offset);
         break;

      case RETRO_VFS_SEEK_POSITION_END:
         if (stream->file_handle != INVALID_HANDLE_VALUE)
            SetFilePointerEx(stream->file_handle, _offset, NULL, FILE_END);
         else
            stream->fp->Seek(stream->fp->Size - offset);
         break;
   }

   /* For simplicity always flush the buffer on seek */
   stream->buffer_left = 0;

   return 0;
}

int64_t retro_vfs_file_read_impl(
      libretro_vfs_implementation_file *stream, void *s, uint64_t len)
{
   int64_t ret;
   int64_t bytes_read = 0;
   IBuffer^ buffer;

   if (!stream || (!stream->fp && stream->file_handle == INVALID_HANDLE_VALUE) || !s)
      return -1;

   if (stream->file_handle != INVALID_HANDLE_VALUE)
   {
      DWORD _bytes_read;
      ReadFile(stream->file_handle, (char*)s, len, &_bytes_read, NULL);
      return (int64_t)_bytes_read;
   }

   if (len <= stream->buffer_size)
   {
      /* Small read, use manually buffered I/O */
      if (stream->buffer_left < len)
      {
         /* Exhaust the buffer */
         memcpy(s,
               &stream->buffer[stream->buffer_fill - stream->buffer_left],
               stream->buffer_left);
         len                 -= stream->buffer_left;
         bytes_read          += stream->buffer_left;
         stream->buffer_left = 0;

         /* Fill buffer */
         stream->buffer_left = RunAsyncAndCatchErrors<int64_t>([&]() {
               return concurrency::create_task(stream->fp->ReadAsync(stream->bufferp, stream->bufferp->Capacity, InputStreamOptions::None)).then([&](IBuffer^ buf) {
                     retro_assert(stream->bufferp == buf);
                     return (int64_t)stream->bufferp->Length;
                     });
               }, -1);
         stream->buffer_fill = stream->buffer_left;

         if (stream->buffer_left == -1)
         {
            stream->buffer_left = 0;
            stream->buffer_fill = 0;
            return -1;
         }

         if (stream->buffer_left < len)
         {
            /* EOF */
            memcpy(&((char*)s)[bytes_read],
                  stream->buffer, stream->buffer_left);
            bytes_read += stream->buffer_left;
            stream->buffer_left = 0;
            return bytes_read;
         }

         memcpy(&((char*)s)[bytes_read], stream->buffer, len);
         bytes_read += len;
         stream->buffer_left -= len;
         return bytes_read;
      }

      /* Internal buffer already contains requested amount */
      memcpy(s,
            &stream->buffer[stream->buffer_fill - stream->buffer_left],
            len);
      stream->buffer_left -= len;
      return len;
   }

   /* Big read exceeding buffer size,
    * exhaust small buffer and read rest in one go */
   memcpy(s, &stream->buffer[stream->buffer_fill - stream->buffer_left], stream->buffer_left);
   len                 -= stream->buffer_left;
   bytes_read          += stream->buffer_left;
   stream->buffer_left  = 0;

   buffer               = CreateNativeBuffer(&((char*)s)[bytes_read], len, 0);
   ret                  = RunAsyncAndCatchErrors<int64_t>([&]() {
         return concurrency::create_task(stream->fp->ReadAsync(buffer, buffer->Capacity - bytes_read, InputStreamOptions::None)).then([&](IBuffer^ buf) {
               retro_assert(buf == buffer);
               return (int64_t)buffer->Length;
               });
         }, -1);

   if (ret == -1)
      return -1;
   return bytes_read + ret;
}

int64_t retro_vfs_file_write_impl(
      libretro_vfs_implementation_file *stream, const void *s, uint64_t len)
{
   IBuffer^ buffer;
   if (!stream || (!stream->fp && stream->file_handle == INVALID_HANDLE_VALUE) || !s)
      return -1;

   if (stream->file_handle != INVALID_HANDLE_VALUE)
   {
      DWORD bytes_written;
      WriteFile(stream->file_handle, s, len, &bytes_written, NULL);
      return (int64_t)bytes_written;
   }

   /* const_cast to remove const modifier is undefined behaviour, but the buffer is only read, should be safe */
   buffer = CreateNativeBuffer(const_cast<void*>(s), len, len);
   return RunAsyncAndCatchErrors<int64_t>([&]() {
         return concurrency::create_task(stream->fp->WriteAsync(buffer)).then([&](unsigned int written) {
               return (int64_t)written;
               });
         }, -1);
}

int retro_vfs_file_flush_impl(libretro_vfs_implementation_file *stream)
{
   if (!stream || (!stream->fp && stream->file_handle == INVALID_HANDLE_VALUE) || !stream->fp)
      return -1;

   if (stream->file_handle != INVALID_HANDLE_VALUE)
   {
      FlushFileBuffers(stream->file_handle);
      return 0;
   }

   return RunAsyncAndCatchErrors<int>([&]() {
         return concurrency::create_task(stream->fp->FlushAsync()).then([&](bool this_value_is_not_even_documented_wtf) {
               /* The bool value may be reporting an error or something, but just leave it alone for now */
               /* https://github.com/MicrosoftDocs/winrt-api/issues/841 */
               return 0;
               });
         }, -1);
}

int retro_vfs_file_remove_impl(const char *path)
{
   BOOL result;
   wchar_t *path_wide;
   Platform::String^ path_str;

   if (!path || !*path)
      return -1;

   path_wide = utf8_to_utf16_string_alloc(path);
   windowsize_path(path_wide);
   path_str  = ref new Platform::String(path_wide);
   free(path_wide);

   /* Try Win32 first, this should work in AppData */
   result = DeleteFileFromAppW(path_str->Data());
   if (result)
      return 0;

   if (GetLastError() == ERROR_FILE_NOT_FOUND)
      return -1;

   /* Fallback to WinRT */
   return RunAsyncAndCatchErrors<int>([&]() {
         return concurrency::create_task(LocateStorageItem<StorageFile>(path_str)).then([&](StorageFile^ file) {
               return file->DeleteAsync(StorageDeleteOption::PermanentDelete);
               }).then([&]() {
                  return 0;
                  });
         }, -1);
}

/* TODO: this may not work if trying to move a directory */
int retro_vfs_file_rename_impl(const char *old_path, const char *new_path)
{
   char new_file_name[PATH_MAX_LENGTH];
   char new_dir_path[PATH_MAX_LENGTH];
   wchar_t *new_file_name_wide;
   wchar_t *old_path_wide, *new_dir_path_wide;
   Platform::String^ old_path_str;
   Platform::String^ new_dir_path_str;
   Platform::String^ new_file_name_str;

   if (!old_path || !*old_path || !new_path || !*new_path)
      return -1;

   new_file_name[0] = '\0';
   new_dir_path [0] = '\0';

   old_path_wide = utf8_to_utf16_string_alloc(old_path);
   old_path_str  = ref new Platform::String(old_path_wide);
   free(old_path_wide);

   fill_pathname_basedir(new_dir_path, new_path, sizeof(new_dir_path));
   new_dir_path_wide = utf8_to_utf16_string_alloc(new_dir_path);
   windowsize_path(new_dir_path_wide);
   new_dir_path_str  = ref new Platform::String(new_dir_path_wide);
   free(new_dir_path_wide);

   fill_pathname_base(new_file_name, new_path, sizeof(new_file_name));
   new_file_name_wide = utf8_to_utf16_string_alloc(new_file_name);
   new_file_name_str  = ref new Platform::String(new_file_name_wide);
   free(new_file_name_wide);

   retro_assert(!old_path_str->IsEmpty() && !new_dir_path_str->IsEmpty() && !new_file_name_str->IsEmpty());

   return RunAsyncAndCatchErrors<int>([&]() {
      concurrency::task<StorageFile^> old_file_task = concurrency::create_task(LocateStorageItem<StorageFile>(old_path_str));
      concurrency::task<StorageFolder^> new_dir_task = concurrency::create_task(LocateStorageItem<StorageFolder>(new_dir_path_str));
      return concurrency::create_task([&] {
         /* Run these two tasks in parallel */
         /* TODO: There may be some cleaner way to express this */
         concurrency::task_group group;
         group.run([&] { return old_file_task; });
         group.run([&] { return new_dir_task; });
         group.wait();
      }).then([&]() {
         return old_file_task.get()->MoveAsync(new_dir_task.get(), new_file_name_str, NameCollisionOption::ReplaceExisting);
      }).then([&]() {
         return 0;
      });
   }, -1);
}

const char *retro_vfs_file_get_path_impl(libretro_vfs_implementation_file *stream)
{
   /* should never happen, do something noisy so caller can be fixed */
   if (!stream)
      abort();
   return stream->orig_path;
}

int retro_vfs_stat_impl(const char *path, int32_t *size)
{
   wchar_t *path_wide;
   Platform::String^ path_str;
   IStorageItem^ item;
   DWORD file_info;
   _WIN32_FILE_ATTRIBUTE_DATA attribdata;

   if (!path || !*path)
      return 0;

   path_wide = utf8_to_utf16_string_alloc(path);
   windowsize_path(path_wide);
   path_str  = ref new Platform::String(path_wide);
   free(path_wide);

   /* Try Win32 first, this should work in AppData */
   if (GetFileAttributesExFromAppW(path_str->Data(), GetFileExInfoStandard, &attribdata))
   {
       file_info = attribdata.dwFileAttributes;
       if (file_info != INVALID_FILE_ATTRIBUTES)
       {
           if (!(file_info & FILE_ATTRIBUTE_DIRECTORY))
           {
               LARGE_INTEGER sz;
               if (size)
               {
                   sz.HighPart = attribdata.nFileSizeHigh;
                   sz.LowPart = attribdata.nFileSizeLow;
               }
           }
           return (file_info & FILE_ATTRIBUTE_DIRECTORY) ? RETRO_VFS_STAT_IS_VALID | RETRO_VFS_STAT_IS_DIRECTORY : RETRO_VFS_STAT_IS_VALID;
       }
   }

   if (GetLastError() == ERROR_FILE_NOT_FOUND)
      return 0;

   /* Fallback to WinRT */
   item = LocateStorageFileOrFolder(path_str);
   if (!item)
      return 0;

   return RunAsyncAndCatchErrors<int>([&]() {
         return concurrency::create_task(item->GetBasicPropertiesAsync()).then([&](BasicProperties^ properties) {
               if (size)
               *size = properties->Size;
               return item->IsOfType(StorageItemTypes::Folder) ? RETRO_VFS_STAT_IS_VALID | RETRO_VFS_STAT_IS_DIRECTORY : RETRO_VFS_STAT_IS_VALID;
               });
         }, 0);
}

int retro_vfs_mkdir_impl(const char *dir)
{
   Platform::String^ parent_path_str;
   Platform::String^ dir_name_str;
   Platform::String^ dir_str;
   wchar_t *dir_name_wide, *parent_path_wide, *dir_wide;
   char *dir_local, *tmp;
   char parent_path[PATH_MAX_LENGTH];
   char dir_name[PATH_MAX_LENGTH];
   BOOL result;
   
   if (!dir || !*dir)
      return -1;

   dir_name[0]      = '\0';

   /* If the path ends with a slash, we have to remove 
    * it for basename to work */
   dir_local        = strdup(dir);
   tmp              = dir_local + strlen(dir_local) - 1;

   if (PATH_CHAR_IS_SLASH(*tmp))
      *tmp          = 0;

   dir_wide         = utf8_to_utf16_string_alloc(dir_local);
   windowsize_path(dir_wide);
   dir_str          = ref new Platform::String(dir_wide);
   free(dir_wide);

   fill_pathname_base(dir_name, dir_local, sizeof(dir_name));
   dir_name_wide    = utf8_to_utf16_string_alloc(dir_name);
   dir_name_str     = ref new Platform::String(dir_name_wide);
   free(dir_name_wide);

   fill_pathname_parent_dir(parent_path, dir_local, sizeof(parent_path));
   parent_path_wide = utf8_to_utf16_string_alloc(parent_path);
   windowsize_path(parent_path_wide);
   parent_path_str  = ref new Platform::String(parent_path_wide);
   free(parent_path_wide);

   retro_assert(!dir_name_str->IsEmpty() 
         && !parent_path_str->IsEmpty());

   free(dir_local);

   /* Try Win32 first, this should work in AppData */
   result = CreateDirectoryFromAppW(dir_str->Data(), NULL);
   if (result)
      return 0;
   
   if (GetLastError() == ERROR_ALREADY_EXISTS)
      return -2;

   /* Fallback to WinRT */
   return RunAsyncAndCatchErrors<int>([&]() {
         return concurrency::create_task(LocateStorageItem<StorageFolder>(
                  parent_path_str)).then([&](StorageFolder^ parent) {
                  return parent->CreateFolderAsync(dir_name_str);
                  }).then([&](concurrency::task<StorageFolder^> new_dir) {
                     try
                     {
                     new_dir.get();
                     }
                     catch (Platform::COMException^ e)
                     {
                     if (e->HResult == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS))
                     return -2;
                     throw;
                     }
                     return 0;
                     });
         }, -1);
}

#ifdef VFS_FRONTEND
struct retro_vfs_dir_handle
#else
struct libretro_vfs_implementation_dir
#endif
{
   IVectorView<IStorageItem^>^ directory;
   IIterator<IStorageItem^>^ entry;
   char *entry_name;
};

libretro_vfs_implementation_dir *retro_vfs_opendir_impl(const char *name, bool include_hidden)
{
   wchar_t *name_wide;
   Platform::String^ name_str;
   libretro_vfs_implementation_dir *rdir;

   if (!name || !*name)
      return NULL;

   rdir = (libretro_vfs_implementation_dir*)calloc(1, sizeof(*rdir));
   if (!rdir)
      return NULL;

   name_wide = utf8_to_utf16_string_alloc(name);
   windowsize_path(name_wide);
   name_str  = ref new Platform::String(name_wide);
   free(name_wide);

   rdir->directory = RunAsyncAndCatchErrors<IVectorView<IStorageItem^>^>([&]() {
      return concurrency::create_task(LocateStorageItem<StorageFolder>(name_str)).then([&](StorageFolder^ folder) {
         return folder->GetItemsAsync();
      });
   }, nullptr);

   if (rdir->directory)
      return rdir;

   free(rdir);
   return NULL;
}

bool retro_vfs_readdir_impl(libretro_vfs_implementation_dir *rdir)
{
   if (!rdir->entry)
   {
      rdir->entry = rdir->directory->First();
      return rdir->entry->HasCurrent;
   }
   return rdir->entry->MoveNext();
}

const char *retro_vfs_dirent_get_name_impl(
      libretro_vfs_implementation_dir *rdir)
{
   if (rdir->entry_name)
      free(rdir->entry_name);
   rdir->entry_name = utf16_to_utf8_string_alloc(
         rdir->entry->Current->Name->Data());
   return rdir->entry_name;
}

bool retro_vfs_dirent_is_dir_impl(libretro_vfs_implementation_dir *rdir)
{
   return rdir->entry->Current->IsOfType(StorageItemTypes::Folder);
}

int retro_vfs_closedir_impl(libretro_vfs_implementation_dir *rdir)
{
   if (!rdir)
      return -1;

   if (rdir->entry_name)
      free(rdir->entry_name);
   rdir->entry     = nullptr;
   rdir->directory = nullptr;

   free(rdir);
   return 0;
}

bool uwp_drive_exists(const char *path)
{
   wchar_t *path_wide;
   Platform::String^ path_str;
   if (!path || !*path)
      return 0;

   path_wide = utf8_to_utf16_string_alloc(path);
   path_str  = ref new Platform::String(path_wide);
   free(path_wide);

   return RunAsyncAndCatchErrors<bool>([&]() {
         return concurrency::create_task(StorageFolder::GetFolderFromPathAsync(path_str)).then([](StorageFolder^ properties) {
               return true;
               });
         }, false);
}

char* uwp_trigger_picker(void)
{
   return RunAsyncAndCatchErrors<char*>([&]() {
      return TriggerPickerAddDialog().then([](Platform::String^ path) {
         return utf16_to_utf8_string_alloc(path->Data());
      });
   }, NULL);
}
