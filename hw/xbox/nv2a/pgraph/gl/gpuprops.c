/*
 * Geforce NV2A PGRAPH OpenGL Renderer
 *
 * Copyright (c) 2012 espes
 * Copyright (c) 2015 Jannik Vogel
 * Copyright (c) 2018-2025 Matt Borgerson
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "debug.h"
#include "renderer.h"

static GPUProperties pgraph_gl_gpu_properties;

static const char *vertex_shader_source =
    "#version 400\n"
    "out vec3 v_fragColor;\n"
    "\n"
    "vec2 positions[11] = vec2[](\n"
    "    vec2(-0.5, -0.75),\n"
    "    vec2(-0.25, -0.25),\n"
    "    vec2(-0.75, -0.25),\n"
    "    vec2(0.25, -0.25),\n"
    "    vec2(0.25, -0.75),\n"
    "    vec2(0.75, -0.25),\n"
    "    vec2(0.75, -0.75),\n"
    "    vec2(-0.75, 0.75),\n"
    "    vec2(-0.75, 0.25),\n"
    "    vec2(-0.25, 0.25),\n"
    "    vec2(-0.25, 0.75)\n"
    ");\n"
    "\n"
    "vec3 colors[11] = vec3[](\n"
    "    vec3(0.0, 0.0, 1.0),\n"
    "    vec3(0.0, 1.0, 0.0),\n"
    "    vec3(0.0, 1.0, 1.0),\n"
    "    vec3(0.0, 0.0, 1.0),\n"
    "    vec3(0.0, 1.0, 0.0),\n"
    "    vec3(0.0, 1.0, 1.0),\n"
    "    vec3(1.0, 0.0, 0.0),\n"
    "    vec3(0.0, 0.0, 1.0),\n"
    "    vec3(0.0, 1.0, 0.0),\n"
    "    vec3(0.0, 1.0, 1.0),\n"
    "    vec3(1.0, 0.0, 0.0)\n"
    ");\n"
    "\n"
    "void main() {\n"
    "    gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);\n"
    "    v_fragColor = colors[gl_VertexID];\n"
    "}\n";

static const char *geometry_shader_source =
    "#version 400\n"
    "layout(triangles) in;\n"
    "layout(triangle_strip, max_vertices = 3) out;\n"
    "out vec3 fragColor;\n"
    "in vec3 v_fragColor[];\n"
    "\n"
    "void main() {\n"
    "    for (int i = 0; i < 3; i++) {\n"
             // This should be just:
             //   gl_Position = gl_in[i].gl_Position;
             //   fragColor = v_fragColor[0];
             // but we work around an Nvidia Cg compiler bug which seems to
             // misdetect above as a passthrough shader and effectively
             // replaces the last line with "fragColor = v_fragColor[i];".
             // Doing redundant computation seems to fix it.
             // TODO: what is the minimal way to avoid the bug?
    "        gl_Position = gl_in[i].gl_Position + vec4(1.0/16384.0, 1.0/16384.0, 0.0, 0.0);\n"
    "        precise vec3 color = v_fragColor[0]*(0.999 + gl_in[i].gl_Position.x/16384.0) + v_fragColor[1]*0.00005 + v_fragColor[2]*0.00005;\n"
    "        fragColor = color;\n"
    "        EmitVertex();\n"
    "    }\n"
    "    EndPrimitive();\n"
    "}\n";

static const char *fragment_shader_source =
    "#version 400\n"
    "out vec4 outColor;\n"
    "in vec3 fragColor;\n"
    "\n"
    "void main() {\n"
    "    outColor = vec4(fragColor, 1.0);\n"
    "}\n";

static GLuint compile_shader(GLenum type, const char *source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        log[sizeof(log) - 1] = '\0';
        fprintf(stderr, "GL shader type %d compilation failed: %s\n", type,
                log);
        assert(false);
    }

    return shader;
}

static GLuint create_program(const char *vert_source, const char *geom_source,
                             const char *frag_source)
{
    GLuint vert_shader = compile_shader(GL_VERTEX_SHADER, vert_source);
    GLuint geom_shader = compile_shader(GL_GEOMETRY_SHADER, geom_source);
    GLuint frag_shader = compile_shader(GL_FRAGMENT_SHADER, frag_source);

    GLuint shader_prog = glCreateProgram();
    glAttachShader(shader_prog, vert_shader);
    glAttachShader(shader_prog, geom_shader);
    glAttachShader(shader_prog, frag_shader);
    glLinkProgram(shader_prog);

    GLint success;
    glGetProgramiv(shader_prog, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(shader_prog, sizeof(log), NULL, log);
        log[sizeof(log) - 1] = '\0';
        fprintf(stderr, "GL shader linking failed: %s\n", log);
        assert(false);
    }

    glDeleteShader(vert_shader);
    glDeleteShader(geom_shader);
    glDeleteShader(frag_shader);

    return shader_prog;
}

static void check_gl_error(const char *context)
{
    GLenum err;
    int limit = 10;

    while ((err = glGetError()) != GL_NO_ERROR) {
        fprintf(stderr, "GPU properties OpenGL error 0x%X in %s\n", err,
                context);
        if (--limit <= 0) {
            fprintf(
                stderr,
                "Too many OpenGL errors in %s — possible infinite error loop\n",
                context);
            break;
        }
    }
}

static uint8_t *render_geom_shader_triangles(int width, int height)
{
    // Create the framebuffer and renderbuffer for it
    GLuint fbo, rbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
    check_gl_error("glRenderbufferStorage");
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_RENDERBUFFER, rbo);
    check_gl_error("glFramebufferRenderbuffer");

    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    GLuint shader_prog = create_program(
        vertex_shader_source, geometry_shader_source, fragment_shader_source);
    assert(shader_prog != 0);

    glUseProgram(shader_prog);
    check_gl_error("glUseProgram");
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    check_gl_error("glClear");

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glProvokingVertex(GL_FIRST_VERTEX_CONVENTION);
    glViewport(0, 0, width, height);
    check_gl_error("state setup");

    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    check_gl_error("glBindVertexArray");
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDrawArrays(GL_TRIANGLE_STRIP, 3, 4);
    glDrawArrays(GL_TRIANGLE_FAN, 7, 4);
    check_gl_error("glDrawArrays");
    glFinish(); // glFinish should be unnecessary

    void *pixels = g_malloc(width * height * 4);
    assert(pixels != NULL);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    check_gl_error("glReadPixels");

    glBindVertexArray(0);
    glDeleteVertexArrays(1, &vao);
    glUseProgram(0);
    glDeleteProgram(shader_prog);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glDeleteRenderbuffers(1, &rbo);

    return (uint8_t *)pixels;
}

static bool colors_match(int r1, int g1, int b1, int r2, int g2, int b2)
{
    int dr = r1 - r2;
    int dg = g1 - g2;
    int db = b1 - b2;

    return (dr * dr + dg * dg + db * db) <= 16;
}

static int get_color_index(uint8_t *pixel)
{
    int r = pixel[0];
    int g = pixel[1];
    int b = pixel[2];

    if (colors_match(r, g, b, 0, 0, 255)) {
        return 0;
    } else if (colors_match(r, g, b, 0, 255, 0)) {
        return 1;
    } else if (colors_match(r, g, b, 0, 255, 255)) {
        return 2;
    } else if (colors_match(r, g, b, 255, 0, 0)) {
        return 3;
    } else {
        return -1;
    }
}

static int calc_offset_from_ndc(float x, float y, int width, int height)
{
    int x0 = (int)((x + 1.0f) * width * 0.5f);
    int y0 = (int)((y + 1.0f) * height * 0.5f);

    x0 = MAX(x0, 0);
    y0 = MAX(y0, 0);
    x0 = MIN(x0, width - 1);
    y0 = MIN(y0, height - 1);

    return y0 * width + x0;
}

static void determine_triangle_winding_order(uint8_t *pixels, int width,
                                             int height, GPUProperties *props)
{
    uint8_t *tri_pix =
        pixels + calc_offset_from_ndc(-0.5f, -0.5f, width, height) * 4;
    uint8_t *strip0_pix =
        pixels + calc_offset_from_ndc(0.417f, -0.417f, width, height) * 4;
    uint8_t *strip1_pix =
        pixels + calc_offset_from_ndc(0.583f, -0.583f, width, height) * 4;
    uint8_t *fan_pix =
        pixels + calc_offset_from_ndc(-0.583f, 0.417f, width, height) * 4;
    uint8_t *fan2_pix =
        pixels + calc_offset_from_ndc(-0.417f, 0.583f, width, height) * 4;

    int tri_rot = get_color_index(tri_pix);
    if (tri_rot < 0 || tri_rot > 2) {
        fprintf(stderr,
                "Could not determine triangle rotation, got color: R=%d, G=%d, "
                "B=%d\n",
                tri_pix[0], tri_pix[1], tri_pix[2]);
        tri_rot = 0;
    }
    props->geom_shader_winding.tri = tri_rot;

    int strip0_rot = get_color_index(strip0_pix);
    if (strip0_rot < 0 || strip0_rot > 2) {
        fprintf(stderr,
                "Could not determine triangle strip0 rotation, got color: "
                "R=%d, G=%d, B=%d\n",
                strip0_pix[0], strip0_pix[1], strip0_pix[2]);
        strip0_rot = 0;
    }
    int strip1_rot = get_color_index(strip1_pix) - 1;
    if (strip1_rot < 0 || strip1_rot > 2) {
        fprintf(stderr,
                "Could not determine triangle strip1 rotation, got color: "
                "R=%d, G=%d, B=%d\n",
                strip1_pix[0], strip1_pix[1], strip1_pix[2]);
        strip1_rot = 0;
    }
    props->geom_shader_winding.tri_strip0 = strip0_rot;
    props->geom_shader_winding.tri_strip1 = (3 - strip1_rot) % 3;

    int fan_rot = get_color_index(fan_pix);
    int fan2_rot = get_color_index(fan2_pix);
    if (fan2_rot == 0) {
        fan2_rot = 1;
    }
    fan2_rot--;
    if (fan_rot != fan2_rot) {
        fprintf(stderr,
                "Unexpected inconsistency in triangle fan winding, got colors: "
                "R=%d, G=%d, B=%d and R=%d, G=%d, B=%d\n",
                fan_pix[0], fan_pix[1], fan_pix[2], fan2_pix[0], fan2_pix[1],
                fan2_pix[2]);
        fan_rot = 1;
    }
    if (fan_rot < 0 || fan_rot > 2) {
        fprintf(stderr,
                "Could not determine triangle fan rotation, got color: R=%d, "
                "G=%d, B=%d\n",
                fan_pix[0], fan_pix[1], fan_pix[2]);
        fan_rot = 1;
    }
    props->geom_shader_winding.tri_fan = (fan_rot + 2) % 3;
}

void pgraph_gl_determine_gpu_properties(void)
{
    const int width = 640;
    const int height = 480;

    uint8_t *pixels = render_geom_shader_triangles(width, height);
    determine_triangle_winding_order(pixels, width, height,
                                     &pgraph_gl_gpu_properties);
    g_free(pixels);

    fprintf(stderr, "GL geometry shader winding: %d, %d, %d, %d\n",
            pgraph_gl_gpu_properties.geom_shader_winding.tri,
            pgraph_gl_gpu_properties.geom_shader_winding.tri_strip0,
            pgraph_gl_gpu_properties.geom_shader_winding.tri_strip1,
            pgraph_gl_gpu_properties.geom_shader_winding.tri_fan);
}

GPUProperties *pgraph_gl_get_gpu_properties(void)
{
    return &pgraph_gl_gpu_properties;
}
