/*
 * Copyright © 2020 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: William Wold <william.wold@canonical.com>
 */

#include "surface_builder.h"
#include "xdg_shell_stable.h"

auto wlcs::SurfaceBuilder::all_surface_types() -> std::vector<std::shared_ptr<SurfaceBuilder>>
{
    return {
        std::make_shared<WlShellSurfaceBuilder>(),
        std::make_shared<XdgV6SurfaceBuilder>(),
        std::make_shared<XdgStableSurfaceBuilder>(0, 0, 0, 0),
        std::make_shared<XdgStableSurfaceBuilder>(12, 5, 20, 6),
        std::make_shared<SubsurfaceBuilder>(std::make_pair(0, 0)),
        std::make_shared<SubsurfaceBuilder>(std::make_pair(7, 12))};
}

auto wlcs::SurfaceBuilder::toplevel_surface_types() -> std::vector<std::shared_ptr<SurfaceBuilder>>
{
    return {
        std::make_shared<WlShellSurfaceBuilder>(),
        std::make_shared<XdgV6SurfaceBuilder>(),
        std::make_shared<XdgStableSurfaceBuilder>(0, 0, 0, 0)};
}

auto wlcs::SurfaceBuilder::surface_builder_to_string(
    testing::TestParamInfo<std::shared_ptr<SurfaceBuilder>> builder) -> std::string
{
    return builder.param->name;
}

auto wlcs::WlShellSurfaceBuilder::build(
    wlcs::Server& server,
    wlcs::Client& client,
    std::pair<int, int> position,
    std::pair<int, int> size) const -> std::unique_ptr<wlcs::Surface>
{
    auto surface = std::make_unique<wlcs::Surface>(
        client.create_wl_shell_surface(size.first, size.second));
    server.move_surface_to(*surface, position.first, position.second);
    return surface;
}

auto wlcs::XdgV6SurfaceBuilder::build(
    wlcs::Server& server,
    wlcs::Client& client,
    std::pair<int, int> position,
    std::pair<int, int> size) const -> std::unique_ptr<wlcs::Surface>
{
    auto surface = std::make_unique<wlcs::Surface>(
        client.create_xdg_shell_v6_surface(size.first, size.second));
    server.move_surface_to(*surface, position.first, position.second);
    return surface;
}

wlcs::XdgStableSurfaceBuilder::XdgStableSurfaceBuilder(int left_offset, int top_offset, int right_offset, int bottom_offset)
    : SurfaceBuilder{"xdg_surface_stable" +
          ((left_offset || top_offset || right_offset || bottom_offset) ? ("_" +
              std::to_string(left_offset) + "_" +
              std::to_string(top_offset) + "_" +
              std::to_string(right_offset) + "_" +
              std::to_string(bottom_offset)) : "")},
      left_offset{left_offset},
      top_offset{top_offset},
      right_offset{right_offset},
      bottom_offset{bottom_offset}
{
}

auto wlcs::XdgStableSurfaceBuilder::build(
    wlcs::Server& server,
    wlcs::Client& client,
    std::pair<int, int> position,
    std::pair<int, int> size) const -> std::unique_ptr<wlcs::Surface>
{
    auto surface = std::make_unique<wlcs::Surface>(client);
    auto xdg_surface = std::make_shared<wlcs::XdgSurfaceStable>(client, *surface);
    auto xdg_toplevel = std::make_shared<wlcs::XdgToplevelStable>(*xdg_surface);
    // The logical window will be shrunk and moved based on the offset, but the underlying surface will not
    xdg_surface_set_window_geometry(
        *xdg_surface,
        left_offset, top_offset,
        size.first - left_offset - right_offset, size.second -bottom_offset - top_offset);
    surface->attach_visible_buffer(size.first, size.second);
    surface->run_on_destruction(
        [xdg_surface, xdg_toplevel]() mutable
        {
            xdg_toplevel.reset();
            xdg_surface.reset();
        });
    server.move_surface_to(*surface, position.first + left_offset, position.second + top_offset);
    return surface;
}

wlcs::SubsurfaceBuilder::SubsurfaceBuilder(std::pair<int, int> offset)
    : SurfaceBuilder{
        "subsurface_at_x" +
        std::to_string(offset.first) +
        "_y" +
        std::to_string(offset.second)},
        offset{offset}
{
}

auto wlcs::SubsurfaceBuilder::build(
    wlcs::Server& server,
    wlcs::Client& client,
    std::pair<int, int> position,
    std::pair<int, int> size) const -> std::unique_ptr<wlcs::Surface>
{
    auto main_surface = std::make_shared<wlcs::Surface>(
        client.create_visible_surface(80, 50));
    server.move_surface_to(
        *main_surface,
        position.first - offset.first,
        position.second - offset.second);
    client.run_on_destruction(
        [main_surface]() mutable
        {
            main_surface.reset();
        });
    auto subsurface = std::make_unique<wlcs::Subsurface>(wlcs::Subsurface::create_visible(
        *main_surface,
        offset.first, offset.second,
        size.first, size.second));
    // if subsurface is sync, tests would have to commit the parent to modify it
    // this is inconvenient to do in a generic way, so we make it desync
    wl_subsurface_set_desync(*subsurface);
    return subsurface;
}

std::ostream& std::operator<<(std::ostream& out, std::shared_ptr<wlcs::SurfaceBuilder> const& param)
{
    return out << param->name;
}
