/*
 * Copyright © 2018, 2019 Christian Persch
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <memory>
#include <vector>

#include <glib.h>
#include <gio/gio.h>

#include "vtepty.h"
#include "vteptyinternal.hh"

#include "glib-glue.hh"
#include "libc-glue.hh"
#include "refptr.hh"

namespace vte::base {

class SpawnContext {
public:
        using child_setup_type = void(*)(void*);

private:

        vte::glib::RefPtr<VtePty> m_pty{};

        vte::glib::StringPtr m_cwd{};
        vte::glib::StringPtr m_fallback_cwd{};
        vte::glib::StringPtr m_arg0{};
        vte::glib::StrvPtr m_argv{};
        vte::glib::StrvPtr m_envv{};

        std::vector<vte::libc::FD> m_fds{};

        // these 3 are placeholder elements for the PTY peer fd being mapped to 0, 1, 2 later
        // we preallocate this here so that the child setup function doesn't do any
        // allocations
        std::vector<std::pair<int,int>> m_fd_map{{-1, 0}, {-1, 1}, {-1, 2}};

        child_setup_type m_child_setup{nullptr};
        std::unique_ptr<void, void(*)(void*)> m_child_setup_data{nullptr, nullptr};

        bool m_inherit_environ{true};
        bool m_systemd_scope{true};
        bool m_require_systemd_scope{false};
        bool m_search_path{false};

public:
        SpawnContext() = default;
        ~SpawnContext() = default;

        SpawnContext(SpawnContext const&) = delete;
        SpawnContext(SpawnContext&&) = default;
        SpawnContext operator=(SpawnContext const&) = delete;
        SpawnContext operator=(SpawnContext&&) = delete;

        void set_cwd(char const* cwd)
        {
                m_cwd = vte::glib::dup_string(cwd);
        }

        void set_fallback_cwd(char const* cwd)
        {
                m_fallback_cwd = vte::glib::dup_string(cwd);
        }

        void set_argv(char const* arg0,
                      char const* const* argv)
        {
                m_arg0 = vte::glib::dup_string(arg0);
                m_argv = vte::glib::dup_strv(argv);
        }

        void set_environ(char const* const* envv)
        {
                m_envv = vte::glib::dup_strv(envv);
        }

        void setenv(char const* env,
                    char const* value,
                    bool overwrite = true)
        {
                m_envv = vte::glib::take_strv(g_environ_setenv(m_envv.release(), env, value, overwrite));
        }

        void unsetenv(char const* env)
        {
                m_envv = vte::glib::take_strv(g_environ_unsetenv(m_envv.release(), env));
        }

        void set_pty(vte::glib::RefPtr<VtePty>&& pty)
        {
                m_pty = std::move(pty);
        }

        void set_child_setup(child_setup_type func,
                             void* data,
                             void(*destroy)(void*))
        {
                m_child_setup = func;
                if (destroy)
                        m_child_setup_data = {data, destroy};
                else
                        m_child_setup_data = {data, [](auto ptr){}};
        }

        void add_fds(int const* fds,
                     int n_fds)
        {
                m_fds.reserve(m_fds.size() + n_fds);
                for (auto i = int{0}; i < n_fds; ++i)
                        m_fds.emplace_back(fds[i]);
        }

        void add_map_fds(int const* fds,
                         int n_fds,
                         int const* map_fds,
                         int n_map_fds)
        {
                m_fd_map.reserve(m_fd_map.size() + n_fds);
                for (auto i = int{0}; i < n_fds; ++i)
                        m_fd_map.emplace_back(fds[i], i < n_map_fds ? map_fds[i] : -1);
        }

        void add_map_fd(int fd,
                        int map_to)
        {
                add_map_fds(&fd, 1, &map_to, 1);
        }

        void set_no_inherit_environ()    noexcept { m_inherit_environ = false;      }
        void set_no_systemd_scope()      noexcept { m_systemd_scope = false;        }
        void set_require_systemd_scope() noexcept { m_require_systemd_scope = true; }
        void set_search_path()           noexcept { m_search_path = true;           }

        auto arg0()         const noexcept { return m_arg0.get(); }
        auto argv()         const noexcept { return m_argv.get(); }
        auto cwd()          const noexcept { return m_cwd.get();  }
        auto fallback_cwd() const noexcept { return m_fallback_cwd.get(); }
        auto environ()      const noexcept { return m_envv.get(); }

        auto pty_wrapper() const noexcept { return m_pty.get();  }
        auto pty() const noexcept { return _vte_pty_get_impl(pty_wrapper()); }

        constexpr auto inherit_environ()       const noexcept { return m_inherit_environ;       }
        constexpr auto systemd_scope()         const noexcept { return m_systemd_scope;         }
        constexpr auto require_systemd_scope() const noexcept { return m_require_systemd_scope; }

        char const* search_path() const noexcept;
        size_t workbuf_size() const noexcept;

        void prepare_environ();

        enum class ExecError {
                CHDIR,
                DUP,
                DUP2,
                EXEC,
                FDWALK,
                GETPTPEER,
                SCTTY,
                SETSID,
                SIGMASK,
                UNSET_CLOEXEC,
        };

        ExecError exec(vte::libc::FD& child_report_error_pipe_write,
                       void* workbuf,
                       size_t workbufsize) noexcept;

}; // class SpawnContext

class SpawnOperation {
private:
        int const default_timeout = 30000; /* ms */

        SpawnContext m_context{};
        int m_timeout{default_timeout};
        vte::glib::RefPtr<GCancellable> m_cancellable{};

        GPollFD m_cancellable_pollfd{-1, 0, 0};
        vte::libc::FD m_child_report_error_pipe_read{};
        pid_t m_pid{-1};
        bool m_kill_pid{true};

        auto& context() noexcept { return m_context; }

        bool prepare(vte::glib::Error& error);
        bool run(vte::glib::Error& error) noexcept;

        void run_in_thread(GTask* task) noexcept;

        auto release_pid() noexcept { auto pid = m_pid; m_pid = -1; return pid; }

        static void delete_cb(void* that)
        {
                /* Take ownership */
                auto op = std::unique_ptr<SpawnOperation>
                        (reinterpret_cast<SpawnOperation*>(that));
        }

        static void run_in_thread_cb(GTask* task,
                                     gpointer source_object,
                                     gpointer that,
                                     GCancellable* cancellable)
        {
                reinterpret_cast<SpawnOperation*>(that)->run_in_thread(task);
        }

public:
        SpawnOperation(SpawnContext&& context,
                       int timeout,
                       GCancellable* cancellable)
                : m_context{std::move(context)},
                  m_timeout{timeout >= 0 ? timeout : default_timeout},
                  m_cancellable{vte::glib::make_ref(cancellable)}
        {
                m_context.prepare_environ();
        }

        ~SpawnOperation();

        SpawnOperation(SpawnOperation const&) = delete;
        SpawnOperation(SpawnOperation&&) = delete;
        SpawnOperation operator=(SpawnOperation const&) = delete;
        SpawnOperation operator=(SpawnOperation&&) = delete;

        static void run_async(std::unique_ptr<SpawnOperation> op,
                              void *source_tag,
                              GAsyncReadyCallback callback,
                              void* user_data);

        static bool run_sync(SpawnOperation& op,
                             GPid* pid,
                             vte::glib::Error& error);

}; // class SpawnOperation

} // namespace vte::base
