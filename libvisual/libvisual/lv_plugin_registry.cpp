#include "config.h"
#include "lv_plugin_registry.h"

#include "lv_common.h"
#include "lv_actor.h"
#include "lv_input.h"
#include "lv_morph.h"
#include "lv_transform.h"
#include "lv_util.hpp"
#include "lv_libvisual.h"
#include "lv_module.hpp"

#include <vector>
#include <unordered_map>
#include <cstdlib>

namespace LV {

  namespace {
    typedef std::unordered_map<PluginType, PluginList, std::hash<int>> PluginListMap;
  }

  class PluginRegistry::Impl
  {
  public:

      std::vector<std::string> plugin_paths;

      PluginListMap plugin_list_map;

      PluginList get_plugins_from_dir (std::string const& dir) const;
  };

  PluginRef* load_plugin_ref (std::string const& plugin_path)
  {
      // NOTE: This does not check if a plugin has already been loaded

      try {
          auto module = Module::load (plugin_path);

          auto plugin_version = static_cast<int*> (module->get_symbol (VISUAL_PLUGIN_VERSION_TAG));

          if (!plugin_version || *plugin_version != VISUAL_PLUGIN_API_VERSION) {
              visual_log (VISUAL_LOG_ERROR, "Plugin %s is not compatible with version %s of libvisual",
                          plugin_path.c_str (), visual_get_version ());
              return nullptr;
          }

          auto get_plugin_info =
              reinterpret_cast<VisPluginGetInfoFunc> (module->get_symbol ("get_plugin_info"));

          if (!get_plugin_info) {
              visual_log (VISUAL_LOG_ERROR, "Cannot get function that returns plugin info");
              return nullptr;
          }

          auto plugin_info = get_plugin_info ();

          if (!plugin_info) {
              visual_log (VISUAL_LOG_ERROR, "Cannot get plugin info");
              return nullptr;
          }

          auto ref = new PluginRef;
          ref->info   = plugin_info;
          ref->file   = plugin_path;
          ref->module = module;

          return ref;
      }
      catch (LV::Error& error) {
          visual_log (VISUAL_LOG_ERROR, "Cannot load plugin (%s): %s", plugin_path.c_str (), error.what());
          return nullptr;
      }
  }

  template <>
  LV_API PluginRegistry* Singleton<PluginRegistry>::m_instance = nullptr;

  void PluginRegistry::init ()
  {
      if (!m_instance)
          m_instance = new PluginRegistry;
  }

  PluginRegistry::PluginRegistry ()
      : m_impl (new Impl)
  {
      visual_log (VISUAL_LOG_DEBUG, "Initializing plugin registry");

      // Add the standard plugin paths
      add_path (VISUAL_PLUGIN_PATH "/actor");
      add_path (VISUAL_PLUGIN_PATH "/input");
      add_path (VISUAL_PLUGIN_PATH "/morph");
      add_path (VISUAL_PLUGIN_PATH "/transform");

#if defined(VISUAL_OS_POSIX)
      // Add homedirectory plugin paths
      auto const home_env = std::getenv ("HOME");

      if (home_env) {
          std::string home_dir {home_env};

          add_path (home_dir + "/.libvisual/actor");
          add_path (home_dir + "/.libvisual/input");
          add_path (home_dir + "/.libvisual/morph");
          add_path (home_dir + "/.libvisual/transform");
      }
#endif
  }

  PluginRegistry::~PluginRegistry ()
  {
      // empty;
  }

  void PluginRegistry::add_path (std::string const& path)
  {
      visual_log (VISUAL_LOG_INFO, "Adding to plugin search path: %s", path.c_str());

      m_impl->plugin_paths.push_back (path);

      auto plugins = m_impl->get_plugins_from_dir (path);

      for (auto& plugin : plugins)
      {
          auto& list = m_impl->plugin_list_map[plugin.info->type];
          list.push_back (plugin);
      }
  }

  PluginRef const* PluginRegistry::find_plugin (PluginType type, std::string const& name) const
  {
      for (auto const& plugin : get_plugins_by_type (type)) {
          if (name == plugin.info->plugname) {
              return &plugin;
          }
      }

      return nullptr;
  }

  bool PluginRegistry::has_plugin (PluginType type, std::string const& name) const
  {
      return find_plugin (type, name) != nullptr;
  }

  PluginList const& PluginRegistry::get_plugins_by_type (PluginType type) const
  {
      static PluginList empty;

      auto match = m_impl->plugin_list_map.find (type);
      if (match == m_impl->plugin_list_map.end ())
          return empty;

      return match->second;
  }

  VisPluginInfo const* PluginRegistry::get_plugin_info (PluginType type, std::string const& name) const
  {
      auto ref = find_plugin (type, name);

      return ref ? ref->info : nullptr;
  }

  PluginList PluginRegistry::Impl::get_plugins_from_dir (std::string const& dir) const
  {
      PluginList list;
      list.reserve (30);

      for_each_file_in_dir (dir,
                            [&] (std::string const& path) -> bool {
                                return str_has_suffix (path, Module::path_suffix ());
                            },
                            [&] (std::string const& path) -> bool {
                                auto ref = load_plugin_ref (path);

                                if (ref) {
                                    visual_log (VISUAL_LOG_DEBUG, "Adding plugin: %s", ref->info->name);
                                    list.push_back (*ref);
                                    delete ref;
                                }

                                return true;
                            });

      return list;
  }

} // LV namespace