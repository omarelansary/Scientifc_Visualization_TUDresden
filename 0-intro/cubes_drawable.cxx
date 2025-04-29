// This source code is property of the Computer Graphics and Visualization chair of the
// TU Dresden. Do not distribute! 
// Copyright (C) CGV TU Dresden - All Rights Reserved
//
// The main file of the plugin. It defines a class that demonstrates how to register with
// the scene graph, drawing primitives, creating a GUI, using a config file and various
// other parts of the framework.

// Framework core
#include <cgv/base/register.h>
#include <cgv/gui/provider.h>
#include <cgv/gui/trigger.h>
#include <cgv/render/drawable.h>
#include <cgv/render/shader_program.h>
#include <cgv/render/vertex_buffer.h>
#include <cgv/render/attribute_array_binding.h>
#include <cgv/math/ftransform.h>

// Framework standard plugins
#include <cgv_gl/gl/gl.h>

// Local includes
#include "cubes_fractal.h"


// ************************************************************************************/
// Task 1.2a: Create a drawable that provides a (for now, empty) GUI and supports
//            reflection, so that its properties can be set via config file.
//
// Task 1.2b: Utilize the cubes_fractal class to render a fractal of hierarchically
//            transformed cubes. Expose its recursion depth and color properties to GUI
//            manipulation and reflection. Set reasonable values via the config
//            file.
//
// Task 1.2c: Implement an option (configurable via GUI and config file) to use a vertex
//            array object for rendering the cubes. The vertex array functionality 
//            should support (again, configurable via GUI and config file) both
//            interleaved (as in cgv_demo.cpp) and non-interleaved attributes.


class cubes_drawable :
    public cgv::base::base,
    public cgv::gui::provider,
    public cgv::render::drawable
{
protected:
    // Task 1.2b: Fractal properties
    cubes_fractal fractal;
    unsigned recursion_depth = 3;
    int storage_mode = 0;
    cgv::media::color<float> root_color = { 1.0f, 0.5f, 0.2f };

    // Task 1.2c: Vertex array options
    //enum StorageMode { BUILTIN, INTERLEAVED, NON_INTERLEAVED } storage_mode = BUILTIN;
    cgv::render::vertex_buffer vb_interleaved, vb_non_interleaved;
    cgv::render::attribute_array_binding aab_interleaved, aab_non_interleaved;

    // Vertex structure for interleaved storage
    struct vertex {
        cgv::vec3 pos;
        cgv::vec3 normal;
    };



public:
    cubes_drawable()
    {
        root_color = cgv::media::color<float>(0.8f, 0.2f, 0.2f);
    }

    std::string get_type_name() const { return "cubes_drawable"; }

    bool self_reflect(cgv::reflect::reflection_handler& rh)
    {
        return
            rh.reflect_member("recursion_depth", recursion_depth) &&
            rh.reflect_member("root_color.R", root_color.R()) &&
            rh.reflect_member("root_color.G", root_color.G()) &&
            rh.reflect_member("root_color.B", root_color.B()) &&
            rh.reflect_member("storage_mode", storage_mode);
    }

    void on_set(void* member_ptr)
    {
        update_member(member_ptr);
        post_redraw();
    }

    void create_gui()
    {
        add_member_control(this, "Storage Mode", storage_mode, "dropdown","enums='Built-in=0,Interleaved=1,Non-interleaved=2'");
        add_member_control(this, "Recursion Depth", recursion_depth, "value_slider", "min=0;max=5");
        add_member_control(this, "Root Color", root_color);

        add_decorator("Vertex Storage Mode", "heading");
        add_member_control(this, "Mode", storage_mode, "dropdown", "enums='Built-in,Interleaved,Non-interleaved'");
    }

    bool init(cgv::render::context& ctx)
    {
        // Initialize vertex buffers for cube geometry
        init_cube_geometry(ctx);
        return true;
    }

    void init_cube_geometry(cgv::render::context& ctx)
    {
        // Cube vertices with normals (8 vertices)
        std::vector<vertex> vertices = {
            // Front face
            {{-0.5f, -0.5f, 0.5f}, {0,0,1}}, {{0.5f, -0.5f, 0.5f}, {0,0,1}},
            {{0.5f, 0.5f, 0.5f}, {0,0,1}}, {{-0.5f, 0.5f, 0.5f}, {0,0,1}},
            // Back face
            {{-0.5f, -0.5f, -0.5f}, {0,0,-1}}, {{0.5f, -0.5f, -0.5f}, {0,0,-1}},
            {{0.5f, 0.5f, -0.5f}, {0,0,-1}}, {{-0.5f, 0.5f, -0.5f}, {0,0,-1}}
        };

        // Interleaved buffer
        vb_interleaved.create(ctx, vertices.data(), vertices.size());
        aab_interleaved.create(ctx);
        aab_interleaved.set_attribute_array(ctx,
            ctx.ref_surface_shader_program().get_position_index(),
            cgv::render::element_descriptor_traits<cgv::vec3>::get_type_descriptor(vertices[0].pos),
            vb_interleaved, 0, vertices.size(), sizeof(vertex));
        aab_interleaved.set_attribute_array(ctx,
            ctx.ref_surface_shader_program().get_normal_index(),
            cgv::render::element_descriptor_traits<cgv::vec3>::get_type_descriptor(vertices[0].normal),
            vb_interleaved, sizeof(cgv::vec3), vertices.size(), sizeof(vertex));

        // Non-interleaved buffer (positions and normals separate)
        std::vector<cgv::vec3> positions, normals;
        for (const auto& v : vertices) {
            positions.push_back(v.pos);
            normals.push_back(v.normal);
        }

        // For non-interleaved buffer
        vb_non_interleaved.create(ctx, positions.size() * sizeof(cgv::vec3) * 2); // Space for positions + normals
        vb_non_interleaved.replace(ctx, 0, positions.data(), positions.size());
        vb_non_interleaved.replace(ctx, positions.size() * sizeof(cgv::vec3), normals.data(), normals.size());

        aab_non_interleaved.create(ctx);
        aab_non_interleaved.set_attribute_array(ctx,
            ctx.ref_surface_shader_program().get_position_index(),
            cgv::render::element_descriptor_traits<cgv::vec3>::get_type_descriptor(positions[0]),
            vb_non_interleaved, 0, positions.size(), 0);
        aab_non_interleaved.set_attribute_array(ctx,
            ctx.ref_surface_shader_program().get_normal_index(),
            cgv::render::element_descriptor_traits<cgv::vec3>::get_type_descriptor(normals[0]),
            vb_non_interleaved, positions.size() * sizeof(cgv::vec3), normals.size(), 0);
    }

    void draw(cgv::render::context& ctx)
    {
        // Set up shader
        auto& shader = ctx.ref_surface_shader_program();
        shader.enable(ctx);

        // Configure vertex array based on storage mode
        switch (storage_mode) {
        case 1:
            fractal.use_vertex_array(&aab_interleaved, 36, GL_TRIANGLES);
            break;
        case 2:
            fractal.use_vertex_array(&aab_non_interleaved, 36, GL_TRIANGLES);
            break;
        default:
            fractal.use_vertex_array(nullptr, 0, 0);
        }

        // Draw the fractal
        ctx.push_modelview_matrix();
        fractal.draw_recursive(ctx, root_color, recursion_depth);
        ctx.pop_modelview_matrix();

        shader.disable(ctx);
    }
};

// Register the plugin
cgv::base::object_registration<cubes_drawable> cubes_drawable_registration("");

// [END] Tasks 1.2a, 1.2b and 1.2c
// ************************************************************************************/


// ************************************************************************************/
// Task 1.2a: register an instance of your drawable.

// < your code here >
