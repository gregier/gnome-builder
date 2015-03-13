/* ide-log.c
 *
 * Copyright (C) 2009-2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#ifdef __linux__
# include <sys/utsname.h>
# include <sys/types.h>
# include <sys/syscall.h>
#elif defined __FreeBSD__
# include <sys/utsname.h>
#endif /* !__linux__ && !__FreeBSD__ */

#include <glib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "ide-debug.h"
#include "ide-log.h"

static GPtrArray *channels = NULL;
static gchar hostname[64] = "";
static GLogFunc last_handler = NULL;
static int log_verbosity = 0;

G_LOCK_DEFINE (channels_lock);

/**
 * ide_log_get_thread:
 *
 * Retrieves task id for the current thread. This is only supported on Linux.
 * On other platforms, the current current thread pointer is retrieved.
 *
 * Returns: The task id.
 */
static inline gint
ide_log_get_thread (void)
{
#if __linux__
  return (gint) syscall (SYS_gettid);
#else
  return GPOINTER_TO_INT (g_thread_self ());
#endif /* __linux__ */
}

/**
 * ide_log_level_str:
 * @log_level: A #GLogLevelFlags.
 *
 * Retrieves the log level as a string.
 *
 * Returns: A string which shouldn't be modified or freed.
 * Side effects: None.
 */
static inline const gchar *
ide_log_level_str (GLogLevelFlags log_level)
{
#define CASE_LEVEL_STR(_l) case G_LOG_LEVEL_ ## _l: return #_l
  switch (((gulong)log_level & G_LOG_LEVEL_MASK))
    {
    CASE_LEVEL_STR (ERROR);
    CASE_LEVEL_STR (CRITICAL);
    CASE_LEVEL_STR (WARNING);
    CASE_LEVEL_STR (MESSAGE);
    CASE_LEVEL_STR (INFO);
    CASE_LEVEL_STR (DEBUG);
    CASE_LEVEL_STR (TRACE);

    default:
      return "UNKNOWN";
    }
#undef CASE_LEVEL_STR
}

/**
 * ide_log_write_to_channel:
 * @channel: A #GIOChannel.
 * @message: A string log message.
 *
 * Writes @message to @channel and flushes the channel.
 */
static void
ide_log_write_to_channel (GIOChannel  *channel,
                         const gchar *message)
{
  g_io_channel_write_chars (channel, message, -1, NULL, NULL);
  g_io_channel_flush (channel, NULL);
}

/**
 * ide_log_handler:
 * @log_domain: A string containing the log section.
 * @log_level: A #GLogLevelFlags.
 * @message: The string message.
 * @user_data: User data supplied to g_log_set_default_handler().
 *
 * Default log handler that will dispatch log messages to configured logging
 * destinations.
 *
 * Returns: None.
 * Side effects: None.
 */
static void
ide_log_handler (const gchar   *log_domain,
                GLogLevelFlags log_level,
                const gchar   *message,
                gpointer       user_data)
{
  struct timespec ts;
  struct tm tt;
  time_t t;
  const gchar *level;
  gchar ftime[32];
  gchar *buffer;

  if (G_LIKELY (channels->len))
    {
      switch ((int)log_level)
        {
        case G_LOG_LEVEL_MESSAGE:
          if (log_verbosity < 1)
            return;
          break;

        case G_LOG_LEVEL_INFO:
          if (log_verbosity < 2)
            return;
          break;

        case G_LOG_LEVEL_DEBUG:
          if (log_verbosity < 3)
            return;
          break;

        case G_LOG_LEVEL_TRACE:
          if (log_verbosity < 4)
            return;
          break;

        default:
          break;
        }

      level = ide_log_level_str (log_level);
      clock_gettime (CLOCK_REALTIME, &ts);
      t = (time_t) ts.tv_sec;
      tt = *localtime (&t);
      strftime (ftime, sizeof (ftime), "%Y/%m/%d %H:%M:%S", &tt);
      buffer = g_strdup_printf ("%s.%04ld  %s: %22s[%d]: %8s: %s\n",
                                ftime, ts.tv_nsec / 100000,
                                hostname, log_domain,
                                ide_log_get_thread (), level, message);
      G_LOCK (channels_lock);
      g_ptr_array_foreach (channels, (GFunc) ide_log_write_to_channel, buffer);
      G_UNLOCK (channels_lock);
      g_free (buffer);
    }
}

/**
 * ide_log_init:
 * @stdout_: Indicates logging should be written to stdout.
 * @filename: An optional file in which to store logs.
 *
 * Initializes the logging subsystem.
 */
void
ide_log_init (gboolean     stdout_,
              const gchar *filename)
{
  static gsize initialized = FALSE;
  struct utsname u;
  GIOChannel *channel;

  if (g_once_init_enter (&initialized))
    {
      channels = g_ptr_array_new ();
      if (filename)
        {
          channel = g_io_channel_new_file (filename, "a", NULL);
          g_ptr_array_add (channels, channel);
        }
      if (stdout_)
        {
          channel = g_io_channel_unix_new (STDOUT_FILENO);
          g_ptr_array_add (channels, channel);
        }

#if defined __linux__ || defined __FreeBSD__
      uname (&u);
      memcpy (hostname, u.nodename, sizeof (hostname));
#else
# ifdef __APPLE__
      gethostname (hostname, sizeof (hostname));
# else
#  error "Target platform not supported"
# endif /* __APPLE__ */
#endif /* __linux__ */

      g_log_set_default_handler (ide_log_handler, NULL);
      g_once_init_leave (&initialized, TRUE);
    }
}

/**
 * ide_log_shutdown:
 *
 * Cleans up after the logging subsystem.
 */
void
ide_log_shutdown (void)
{
  if (last_handler)
    {
      g_log_set_default_handler (last_handler, NULL);
      last_handler = NULL;
    }
}

/**
 * ide_log_increase_verbosity:
 *
 * Increases the amount of logging that will occur. By default, only
 * warning and above will be displayed.
 *
 * Calling this once will cause G_LOG_LEVEL_MESSAGE to be displayed.
 * Calling this twice will cause G_LOG_LEVEL_INFO to be displayed.
 * Calling this thrice will cause G_LOG_LEVEL_DEBUG to be displayed.
 * Calling this four times will cause G_LOG_LEVEL_TRACE to be displayed.
 *
 * Note that many DEBUG and TRACE level log messages are only compiled into
 * debug builds, and therefore will not be available in release builds.
 *
 * This method is meant to be called for every -v provided on the command
 * line.
 *
 * Calling this method more than four times is acceptable.
 */
void
ide_log_increase_verbosity (void)
{
  log_verbosity++;
}
