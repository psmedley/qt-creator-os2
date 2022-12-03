/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#pragma once

#include <string.h>

#if __has_include(<unistd.h>)
#include <unistd.h>
#endif

#if __has_include(<utime.h>)
#define HAVE_UTIME 1
#endif

#if (_XOPEN_SOURCE >= 500) && !(_POSIX_C_SOURCE >= 200809L) || _DEFAULT_SOURCE || _BSD_SOURCE
#define HAVE_USLEEP 1
#endif

#if _POSIX_C_SOURCE >= 199309L || _XOPEN_SOURCE >= 500
#define HAVE_FDATASYNC 1
#endif

#if _POSIX_C_SOURCE >= 1 || _BSD_SOURCE
#define HAVE_LOCALTIME_R 1
#else
#define HAVE_LOCALTIME_S 1
#endif

#define HAVE_MALLOC_USABLE_SIZE 1
#define HAVE_ISNAN 1

#define SQLITE_THREADSAFE 2
#define SQLITE_ENABLE_FTS5 1
#define SQLITE_ENABLE_UNLOCK_NOTIFY 1
#define SQLITE_ENABLE_JSON1 1
#define SQLITE_DEFAULT_FOREIGN_KEYS 1
#define SQLITE_TEMP_STORE 3
#define SQLITE_DEFAULT_WAL_SYNCHRONOUS 1
#define SQLITE_MAX_WORKER_THREADS 1
#define SQLITE_DEFAULT_MEMSTATUS 0
#define SQLITE_OMIT_DEPRECATED 1
#define SQLITE_OMIT_DECLTYPE 1
#define SQLITE_MAX_EXPR_DEPTH 0
#define SQLITE_OMIT_SHARED_CACHE 1
#define SQLITE_USE_ALLOCA 1
#define SQLITE_ENABLE_MEMORY_MANAGEMENT 1
#define SQLITE_ENABLE_NULL_TRIM 1
#define SQLITE_ALLOW_COVERING_INDEX_SCAN 1
#define SQLITE_OMIT_EXPLAIN 1
#define SQLITE_OMIT_LOAD_EXTENSION 1
#define SQLITE_OMIT_UTF16 1
#define SQLITE_DQS 0
#define SQLITE_ENABLE_STAT4 1
#define SQLITE_DEFAULT_MMAP_SIZE 268435456
#define SQLITE_ENABLE_SESSION 1
#define SQLITE_ENABLE_PREUPDATE_HOOK 1
#define SQLITE_LIKE_DOESNT_MATCH_BLOBS 1
#define SQLITE_OMIT_AUTOINIT 1
#define SQLITE_DEFAULT_CACHE_SIZE -100000
#define SQLITE_OMIT_AUTORESET 1
#define SQLITE_OMIT_EXPLAIN 1
#define SQLITE_OMIT_TRACE 1
#define SQLITE_DEFAULT_LOCKING_MODE 1
#define SQLITE_WIN32_GETVERSIONEX 0

#ifdef SQLITE_STATIC_LIBRARY
    // Make sure to avoid any other sqlite3 static libraries symbols present in the project
    #define sqlite3_aggregate_context                qtc_sqlite3_aggregate_context
    #define sqlite3_auto_extension                   qtc_sqlite3_auto_extension
    #define sqlite3_autovacuum_pages                 qtc_sqlite3_autovacuum_pages
    #define sqlite3_backup_finish                    qtc_sqlite3_backup_finish
    #define sqlite3_backup_init                      qtc_sqlite3_backup_init
    #define sqlite3_backup_pagecount                 qtc_sqlite3_backup_pagecount
    #define sqlite3_backup_remaining                 qtc_sqlite3_backup_remaining
    #define sqlite3_backup_step                      qtc_sqlite3_backup_step
    #define sqlite3_bind_blob                        qtc_sqlite3_bind_blob
    #define sqlite3_bind_blob64                      qtc_sqlite3_bind_blob64
    #define sqlite3_bind_double                      qtc_sqlite3_bind_double
    #define sqlite3_bind_int                         qtc_sqlite3_bind_int
    #define sqlite3_bind_int64                       qtc_sqlite3_bind_int64
    #define sqlite3_bind_null                        qtc_sqlite3_bind_null
    #define sqlite3_bind_parameter_count             qtc_sqlite3_bind_parameter_count
    #define sqlite3_bind_parameter_index             qtc_sqlite3_bind_parameter_index
    #define sqlite3_bind_parameter_name              qtc_sqlite3_bind_parameter_name
    #define sqlite3_bind_pointer                     qtc_sqlite3_bind_pointer
    #define sqlite3_bind_text                        qtc_sqlite3_bind_text
    #define sqlite3_bind_text64                      qtc_sqlite3_bind_text64
    #define sqlite3_bind_value                       qtc_sqlite3_bind_value
    #define sqlite3_bind_zeroblob                    qtc_sqlite3_bind_zeroblob
    #define sqlite3_bind_zeroblob64                  qtc_sqlite3_bind_zeroblob64
    #define sqlite3_blob_bytes                       qtc_sqlite3_blob_bytes
    #define sqlite3_blob_close                       qtc_sqlite3_blob_close
    #define sqlite3_blob_open                        qtc_sqlite3_blob_open
    #define sqlite3_blob_read                        qtc_sqlite3_blob_read
    #define sqlite3_blob_reopen                      qtc_sqlite3_blob_reopen
    #define sqlite3_blob_write                       qtc_sqlite3_blob_write
    #define sqlite3_busy_handler                     qtc_sqlite3_busy_handler
    #define sqlite3_busy_timeout                     qtc_sqlite3_busy_timeout
    #define sqlite3_cancel_auto_extension            qtc_sqlite3_cancel_auto_extension
    #define sqlite3_changes                          qtc_sqlite3_changes
    #define sqlite3_changes64                        qtc_sqlite3_changes64
    #define sqlite3_clear_bindings                   qtc_sqlite3_clear_bindings
    #define sqlite3_close                            qtc_sqlite3_close
    #define sqlite3_close_v2                         qtc_sqlite3_close_v2
    #define sqlite3_collation_needed                 qtc_sqlite3_collation_needed
    #define sqlite3_column_blob                      qtc_sqlite3_column_blob
    #define sqlite3_column_bytes                     qtc_sqlite3_column_bytes
    #define sqlite3_column_bytes16                   qtc_sqlite3_column_bytes16
    #define sqlite3_column_count                     qtc_sqlite3_column_count
    #define sqlite3_column_double                    qtc_sqlite3_column_double
    #define sqlite3_column_int                       qtc_sqlite3_column_int
    #define sqlite3_column_int64                     qtc_sqlite3_column_int64
    #define sqlite3_column_name                      qtc_sqlite3_column_name
    #define sqlite3_column_text                      qtc_sqlite3_column_text
    #define sqlite3_column_type                      qtc_sqlite3_column_type
    #define sqlite3_column_value                     qtc_sqlite3_column_value
    #define sqlite3_commit_hook                      qtc_sqlite3_commit_hook
    #define sqlite3_compileoption_get                qtc_sqlite3_compileoption_get
    #define sqlite3_compileoption_used               qtc_sqlite3_compileoption_used
    #define sqlite3_config                           qtc_sqlite3_config
    #define sqlite3_context_db_handle                qtc_sqlite3_context_db_handle
    #define sqlite3_create_collation                 qtc_sqlite3_create_collation
    #define sqlite3_create_collation_v2              qtc_sqlite3_create_collation_v2
    #define sqlite3_create_filename                  qtc_sqlite3_create_filename
    #define sqlite3_create_function                  qtc_sqlite3_create_function
    #define sqlite3_create_function_v2               qtc_sqlite3_create_function_v2
    #define sqlite3_create_module                    qtc_sqlite3_create_module
    #define sqlite3_create_module_v2                 qtc_sqlite3_create_module_v2
    #define sqlite3_create_window_function           qtc_sqlite3_create_window_function
    #define sqlite3_data_count                       qtc_sqlite3_data_count
    #define sqlite3_database_file_object             qtc_sqlite3_database_file_object
    #define sqlite3_db_cacheflush                    qtc_sqlite3_db_cacheflush
    #define sqlite3_db_config                        qtc_sqlite3_db_config
    #define sqlite3_db_filename                      qtc_sqlite3_db_filename
    #define sqlite3_db_handle                        qtc_sqlite3_db_handle
    #define sqlite3_db_mutex                         qtc_sqlite3_db_mutex
    #define sqlite3_db_readonly                      qtc_sqlite3_db_readonly
    #define sqlite3_db_release_memory                qtc_sqlite3_db_release_memory
    #define sqlite3_db_status                        qtc_sqlite3_db_status
    #define sqlite3_declare_vtab                     qtc_sqlite3_declare_vtab
    #define sqlite3_deserialize                      qtc_sqlite3_deserialize
    #define sqlite3_drop_modules                     qtc_sqlite3_drop_modules
    #define sqlite3_errcode                          qtc_sqlite3_errcode
    #define sqlite3_errmsg                           qtc_sqlite3_errmsg
    #define sqlite3_errstr                           qtc_sqlite3_errstr
    #define sqlite3_exec                             qtc_sqlite3_exec
    #define sqlite3_expanded_sql                     qtc_sqlite3_expanded_sql
    #define sqlite3_extended_errcode                 qtc_sqlite3_extended_errcode
    #define sqlite3_extended_result_codes            qtc_sqlite3_extended_result_codes
    #define sqlite3_file_control                     qtc_sqlite3_file_control
    #define sqlite3_filename_database                qtc_sqlite3_filename_database
    #define sqlite3_filename_journal                 qtc_sqlite3_filename_journal
    #define sqlite3_filename_wal                     qtc_sqlite3_filename_wal
    #define sqlite3_finalize                         qtc_sqlite3_finalize
    #define sqlite3_free                             qtc_sqlite3_free
    #define sqlite3_free_filename                    qtc_sqlite3_free_filename
    #define sqlite3_free_table                       qtc_sqlite3_free_table
    #define sqlite3_get_autocommit                   qtc_sqlite3_get_autocommit
    #define sqlite3_get_auxdata                      qtc_sqlite3_get_auxdata
    #define sqlite3_get_table                        qtc_sqlite3_get_table
    #define sqlite3_hard_heap_limit64                qtc_sqlite3_hard_heap_limit64
    #define sqlite3_initialize                       qtc_sqlite3_initialize
    #define sqlite3_interrupt                        qtc_sqlite3_interrupt
    #define sqlite3_keyword_check                    qtc_sqlite3_keyword_check
    #define sqlite3_keyword_count                    qtc_sqlite3_keyword_count
    #define sqlite3_keyword_name                     qtc_sqlite3_keyword_name
    #define sqlite3_last_insert_rowid                qtc_sqlite3_last_insert_rowid
    #define sqlite3_libversion                       qtc_sqlite3_libversion
    #define sqlite3_libversion_number                qtc_sqlite3_libversion_number
    #define sqlite3_limit                            qtc_sqlite3_limit
    #define sqlite3_log                              qtc_sqlite3_log
    #define sqlite3_malloc                           qtc_sqlite3_malloc
    #define sqlite3_malloc64                         qtc_sqlite3_malloc64
    #define sqlite3_memory_highwater                 qtc_sqlite3_memory_highwater
    #define sqlite3_memory_used                      qtc_sqlite3_memory_used
    #define sqlite3_mprintf                          qtc_sqlite3_mprintf
    #define sqlite3_msize                            qtc_sqlite3_msize
    #define sqlite3_mutex_alloc                      qtc_sqlite3_mutex_alloc
    #define sqlite3_mutex_enter                      qtc_sqlite3_mutex_enter
    #define sqlite3_mutex_free                       qtc_sqlite3_mutex_free
    #define sqlite3_mutex_leave                      qtc_sqlite3_mutex_leave
    #define sqlite3_mutex_try                        qtc_sqlite3_mutex_try
    #define sqlite3_next_stmt                        qtc_sqlite3_next_stmt
    #define sqlite3_open                             qtc_sqlite3_open
    #define sqlite3_open_v2                          qtc_sqlite3_open_v2
    #define sqlite3_os_end                           qtc_sqlite3_os_end
    #define sqlite3_os_init                          qtc_sqlite3_os_init
    #define sqlite3_overload_function                qtc_sqlite3_overload_function
    #define sqlite3_prepare                          qtc_sqlite3_prepare
    #define sqlite3_prepare_v2                       qtc_sqlite3_prepare_v2
    #define sqlite3_prepare_v3                       qtc_sqlite3_prepare_v3
    #define sqlite3_progress_handler                 qtc_sqlite3_progress_handler
    #define sqlite3_randomness                       qtc_sqlite3_randomness
    #define sqlite3_realloc                          qtc_sqlite3_realloc
    #define sqlite3_realloc64                        qtc_sqlite3_realloc64
    #define sqlite3_release_memory                   qtc_sqlite3_release_memory
    #define sqlite3_reset                            qtc_sqlite3_reset
    #define sqlite3_reset_auto_extension             qtc_sqlite3_reset_auto_extension
    #define sqlite3_result_blob                      qtc_sqlite3_result_blob
    #define sqlite3_result_blob64                    qtc_sqlite3_result_blob64
    #define sqlite3_result_double                    qtc_sqlite3_result_double
    #define sqlite3_result_error                     qtc_sqlite3_result_error
    #define sqlite3_result_error_code                qtc_sqlite3_result_error_code
    #define sqlite3_result_error_nomem               qtc_sqlite3_result_error_nomem
    #define sqlite3_result_error_toobig              qtc_sqlite3_result_error_toobig
    #define sqlite3_result_int                       qtc_sqlite3_result_int
    #define sqlite3_result_int64                     qtc_sqlite3_result_int64
    #define sqlite3_result_null                      qtc_sqlite3_result_null
    #define sqlite3_result_pointer                   qtc_sqlite3_result_pointer
    #define sqlite3_result_subtype                   qtc_sqlite3_result_subtype
    #define sqlite3_result_text                      qtc_sqlite3_result_text
    #define sqlite3_result_text64                    qtc_sqlite3_result_text64
    #define sqlite3_result_value                     qtc_sqlite3_result_value
    #define sqlite3_result_zeroblob                  qtc_sqlite3_result_zeroblob
    #define sqlite3_result_zeroblob64                qtc_sqlite3_result_zeroblob64
    #define sqlite3_rollback_hook                    qtc_sqlite3_rollback_hook
    #define sqlite3_serialize                        qtc_sqlite3_serialize
    #define sqlite3_set_authorizer                   qtc_sqlite3_set_authorizer
    #define sqlite3_set_auxdata                      qtc_sqlite3_set_auxdata
    #define sqlite3_set_last_insert_rowid            qtc_sqlite3_set_last_insert_rowid
    #define sqlite3_shutdown                         qtc_sqlite3_shutdown
    #define sqlite3_sleep                            qtc_sqlite3_sleep
    #define sqlite3_snprintf                         qtc_sqlite3_snprintf
    #define sqlite3_soft_heap_limit                  qtc_sqlite3_soft_heap_limit
    #define sqlite3_soft_heap_limit64                qtc_sqlite3_soft_heap_limit64
    #define sqlite3_sourceid                         qtc_sqlite3_sourceid
    #define sqlite3_sql                              qtc_sqlite3_sql
    #define sqlite3_status                           qtc_sqlite3_status
    #define sqlite3_status64                         qtc_sqlite3_status64
    #define sqlite3_step                             qtc_sqlite3_step
    #define sqlite3_stmt_busy                        qtc_sqlite3_stmt_busy
    #define sqlite3_stmt_isexplain                   qtc_sqlite3_stmt_isexplain
    #define sqlite3_stmt_readonly                    qtc_sqlite3_stmt_readonly
    #define sqlite3_stmt_status                      qtc_sqlite3_stmt_status
    #define sqlite3_str_append                       qtc_sqlite3_str_append
    #define sqlite3_str_appendall                    qtc_sqlite3_str_appendall
    #define sqlite3_str_appendchar                   qtc_sqlite3_str_appendchar
    #define sqlite3_str_appendf                      qtc_sqlite3_str_appendf
    #define sqlite3_str_errcode                      qtc_sqlite3_str_errcode
    #define sqlite3_str_finish                       qtc_sqlite3_str_finish
    #define sqlite3_str_length                       qtc_sqlite3_str_length
    #define sqlite3_str_new                          qtc_sqlite3_str_new
    #define sqlite3_str_reset                        qtc_sqlite3_str_reset
    #define sqlite3_str_value                        qtc_sqlite3_str_value
    #define sqlite3_str_vappendf                     qtc_sqlite3_str_vappendf
    #define sqlite3_strglob                          qtc_sqlite3_strglob
    #define sqlite3_stricmp                          qtc_sqlite3_stricmp
    #define sqlite3_strlike                          qtc_sqlite3_strlike
    #define sqlite3_strnicmp                         qtc_sqlite3_strnicmp
    #define sqlite3_system_errno                     qtc_sqlite3_system_errno
    #define sqlite3_table_column_metadata            qtc_sqlite3_table_column_metadata
    #define sqlite3_test_control                     qtc_sqlite3_test_control
    #define sqlite3_threadsafe                       qtc_sqlite3_threadsafe
    #define sqlite3_total_changes                    qtc_sqlite3_total_changes
    #define sqlite3_total_changes64                  qtc_sqlite3_total_changes64
    #define sqlite3_txn_state                        qtc_sqlite3_txn_state
    #define sqlite3_unlock_notify                    qtc_sqlite3_unlock_notify
    #define sqlite3_update_hook                      qtc_sqlite3_update_hook
    #define sqlite3_uri_boolean                      qtc_sqlite3_uri_boolean
    #define sqlite3_uri_int64                        qtc_sqlite3_uri_int64
    #define sqlite3_uri_key                          qtc_sqlite3_uri_key
    #define sqlite3_uri_parameter                    qtc_sqlite3_uri_parameter
    #define sqlite3_user_data                        qtc_sqlite3_user_data
    #define sqlite3_value_blob                       qtc_sqlite3_value_blob
    #define sqlite3_value_bytes                      qtc_sqlite3_value_bytes
    #define sqlite3_value_bytes16                    qtc_sqlite3_value_bytes16
    #define sqlite3_value_double                     qtc_sqlite3_value_double
    #define sqlite3_value_dup                        qtc_sqlite3_value_dup
    #define sqlite3_value_free                       qtc_sqlite3_value_free
    #define sqlite3_value_frombind                   qtc_sqlite3_value_frombind
    #define sqlite3_value_int                        qtc_sqlite3_value_int
    #define sqlite3_value_int64                      qtc_sqlite3_value_int64
    #define sqlite3_value_nochange                   qtc_sqlite3_value_nochange
    #define sqlite3_value_numeric_type               qtc_sqlite3_value_numeric_type
    #define sqlite3_value_pointer                    qtc_sqlite3_value_pointer
    #define sqlite3_value_subtype                    qtc_sqlite3_value_subtype
    #define sqlite3_value_text                       qtc_sqlite3_value_text
    #define sqlite3_value_type                       qtc_sqlite3_value_type
    #define sqlite3_vfs_find                         qtc_sqlite3_vfs_find
    #define sqlite3_vfs_register                     qtc_sqlite3_vfs_register
    #define sqlite3_vfs_unregister                   qtc_sqlite3_vfs_unregister
    #define sqlite3_vmprintf                         qtc_sqlite3_vmprintf
    #define sqlite3_vsnprintf                        qtc_sqlite3_vsnprintf
    #define sqlite3_vtab_collation                   qtc_sqlite3_vtab_collation
    #define sqlite3_vtab_config                      qtc_sqlite3_vtab_config
    #define sqlite3_vtab_nochange                    qtc_sqlite3_vtab_nochange
    #define sqlite3_vtab_on_conflict                 qtc_sqlite3_vtab_on_conflict
    #define sqlite3_wal_autocheckpoint               qtc_sqlite3_wal_autocheckpoint
    #define sqlite3_wal_checkpoint                   qtc_sqlite3_wal_checkpoint
    #define sqlite3_wal_checkpoint_v2                qtc_sqlite3_wal_checkpoint_v2
    #define sqlite3_wal_hook                         qtc_sqlite3_wal_hook
    #define sqlite3_win32_is_nt                      qtc_sqlite3_win32_is_nt
    #define sqlite3_win32_mbcs_to_utf8               qtc_sqlite3_win32_mbcs_to_utf8
    #define sqlite3_win32_mbcs_to_utf8_v2            qtc_sqlite3_win32_mbcs_to_utf8_v2
    #define sqlite3_win32_set_directory              qtc_sqlite3_win32_set_directory
    #define sqlite3_win32_set_directory16            qtc_sqlite3_win32_set_directory16
    #define sqlite3_win32_set_directory8             qtc_sqlite3_win32_set_directory8
    #define sqlite3_win32_sleep                      qtc_sqlite3_win32_sleep
    #define sqlite3_win32_unicode_to_utf8            qtc_sqlite3_win32_unicode_to_utf8
    #define sqlite3_win32_utf8_to_mbcs               qtc_sqlite3_win32_utf8_to_mbcs
    #define sqlite3_win32_utf8_to_mbcs_v2            qtc_sqlite3_win32_utf8_to_mbcs_v2
    #define sqlite3_win32_utf8_to_unicode            qtc_sqlite3_win32_utf8_to_unicode
    #define sqlite3_win32_write_debug                qtc_sqlite3_win32_write_debug
    #define sqlite3_version                          qtc_sqlite3_version
    #define sqlite3_temp_directory                   qtc_sqlite3_temp_directory
    #define sqlite3_data_directory                   qtc_sqlite3_data_directory
    #define sqlite3changeset_finalize                qtc_sqlite3changeset_finalize
    #define sqlite3changeset_op                      qtc_sqlite3changeset_op

    // Redefine structures in order to avoid ODR violations
    #define sqlite3                                  qtc_sqlite3
    #define sqlite3_file                             qtc_sqlite3_file
    #define sqlite3_io_methods                       qtc_sqlite3_io_methods
    #define sqlite3_mutex                            qtc_sqlite3_mutex
    #define sqlite3_api_routines                     qtc_sqlite3_api_routines
    #define sqlite3_vfs                              qtc_sqlite3_vfs
    #define sqlite3_mem_methods                      qtc_sqlite3_mem_methods
    #define sqlite3_stmt                             qtc_sqlite3_stmt
    #define sqlite3_value                            qtc_sqlite3_value
    #define sqlite3_context                          qtc_sqlite3_context
    #define sqlite3_vtab                             qtc_sqlite3_vtab
    #define sqlite3_index_info                       qtc_sqlite3_index_info
    #define sqlite3_vtab_cursor                      qtc_sqlite3_vtab_cursor
    #define sqlite3_module                           qtc_sqlite3_module
    #define sqlite3_mutex_methods                    qtc_sqlite3_mutex_methods
    #define sqlite3_str                              qtc_sqlite3_str
    #define sqlite3_pcache                           qtc_sqlite3_pcache
    #define sqlite3_pcache_page                      qtc_sqlite3_pcache_page
    #define sqlite3_pcache_methods2                  qtc_sqlite3_pcache_methods2
    #define sqlite3_pcache_methods                   qtc_sqlite3_pcache_methods
    #define sqlite3_backup                           qtc_sqlite3_backup
    #define sqlite3_snapshot                         qtc_sqlite3_snapshot
    #define sqlite3_rtree_geometry                   qtc_sqlite3_rtree_geometry
    #define sqlite3_rtree_query_info                 qtc_sqlite3_rtree_query_info
    #define sqlite3_session                          qtc_sqlite3_session
    #define sqlite3_changeset_iter                   qtc_sqlite3_changeset_iter
    #define sqlite3_changegroup                      qtc_sqlite3_changegroup
    #define sqlite3_rebaser                          qtc_sqlite3_rebaser
    #define Fts5ExtensionApi                         qtc_Fts5ExtensionApi
    #define Fts5Context                              qtc_Fts5Context
    #define Fts5PhraseIter                           qtc_Fts5PhraseIter
    #define Fts5Tokenizer                            qtc_Fts5Tokenizer
    #define fts5_tokenizer                           qtc_fts5_tokenizer
    #define fts5_api                                 qtc_fts5_api
#endif
