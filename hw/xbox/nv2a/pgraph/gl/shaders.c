/*
 * Geforce NV2A PGRAPH OpenGL Renderer
 *
 * Copyright (c) 2015 espes
 * Copyright (c) 2015 Jannik Vogel
 * Copyright (c) 2020-2025 Matt Borgerson
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

#include "qemu/osdep.h"
#include "qemu/fast-hash.h"
#include "qemu/mstring.h"
#include <locale.h>

#include "xemu-version.h"
#include "ui/xemu-settings.h"
#include "hw/xbox/nv2a/pgraph/glsl/geom.h"
#include "hw/xbox/nv2a/pgraph/glsl/vsh.h"
#include "hw/xbox/nv2a/pgraph/glsl/psh.h"
#include "hw/xbox/nv2a/pgraph/shaders.h"
#include "hw/xbox/nv2a/pgraph/util.h"
#include "debug.h"
#include "renderer.h"

static GLenum get_gl_primitive_mode(enum ShaderPolygonMode polygon_mode, enum ShaderPrimitiveMode primitive_mode)
{
    if (polygon_mode == POLY_MODE_POINT) {
        return GL_POINTS;
    }

    switch (primitive_mode) {
    case PRIM_TYPE_POINTS: return GL_POINTS;
    case PRIM_TYPE_LINES: return GL_LINES;
    case PRIM_TYPE_LINE_LOOP: return GL_LINE_LOOP;
    case PRIM_TYPE_LINE_STRIP: return GL_LINE_STRIP;
    case PRIM_TYPE_TRIANGLES: return GL_TRIANGLES;
    case PRIM_TYPE_TRIANGLE_STRIP: return GL_TRIANGLE_STRIP;
    case PRIM_TYPE_TRIANGLE_FAN: return GL_TRIANGLE_FAN;
    case PRIM_TYPE_QUADS: return GL_LINES_ADJACENCY;
    case PRIM_TYPE_QUAD_STRIP: return GL_LINE_STRIP_ADJACENCY;
    case PRIM_TYPE_POLYGON:
        if (polygon_mode == POLY_MODE_LINE) {
            return GL_LINE_LOOP;
        } else if (polygon_mode == POLY_MODE_FILL) {
            return GL_TRIANGLE_FAN;
        }

        assert(!"PRIM_TYPE_POLYGON with invalid polygon_mode");
        return 0;
    default:
        assert(!"Invalid primitive_mode");
        return 0;
    }
}

static GLuint create_gl_shader(GLenum gl_shader_type,
                               const char *code,
                               const char *name)
{
    GLint compiled = 0;

    NV2A_GL_DGROUP_BEGIN("Creating new %s", name);

    NV2A_DPRINTF("compile new %s, code:\n%s\n", name, code);

    GLuint shader = glCreateShader(gl_shader_type);
    glShaderSource(shader, 1, &code, 0);
    glCompileShader(shader);

    /* Check it compiled */
    compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLchar* log;
        GLint log_length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
        log = g_malloc(log_length * sizeof(GLchar));
        glGetShaderInfoLog(shader, log_length, NULL, log);
        fprintf(stderr, "%s\n\n" "nv2a: %s compilation failed: %s\n", code, name, log);
        g_free(log);

        NV2A_GL_DGROUP_END();
        abort();
    }

    NV2A_GL_DGROUP_END();

    return shader;
}

static void update_shader_constant_locations(ShaderBinding *binding)
{
    char tmp[64];

    /* set texture samplers */
    for (int i = 0; i < NV2A_MAX_TEXTURES; i++) {
        char samplerName[16];
        snprintf(samplerName, sizeof(samplerName), "texSamp%d", i);
        GLint texSampLoc = glGetUniformLocation(binding->gl_program, samplerName);
        if (texSampLoc >= 0) {
            glUniform1i(texSampLoc, i);
        }
    }

    /* validate the program */
    glValidateProgram(binding->gl_program);
    GLint valid = 0;
    glGetProgramiv(binding->gl_program, GL_VALIDATE_STATUS, &valid);
    if (!valid) {
        GLchar log[1024];
        glGetProgramInfoLog(binding->gl_program, 1024, NULL, log);
        fprintf(stderr, "nv2a: shader validation failed: %s\n", log);
        abort();
    }

    /* lookup fragment shader uniforms */
    for (int i = 0; i < 9; i++) {
        for (int j = 0; j < 2; j++) {
            snprintf(tmp, sizeof(tmp), "c%d_%d", j, i);
            binding->psh_constant_loc[i][j] = glGetUniformLocation(binding->gl_program, tmp);
        }
    }
    binding->alpha_ref_loc = glGetUniformLocation(binding->gl_program, "alphaRef");
    for (int i = 1; i < NV2A_MAX_TEXTURES; i++) {
        snprintf(tmp, sizeof(tmp), "bumpMat%d", i);
        binding->bump_mat_loc[i] = glGetUniformLocation(binding->gl_program, tmp);
        snprintf(tmp, sizeof(tmp), "bumpScale%d", i);
        binding->bump_scale_loc[i] = glGetUniformLocation(binding->gl_program, tmp);
        snprintf(tmp, sizeof(tmp), "bumpOffset%d", i);
        binding->bump_offset_loc[i] = glGetUniformLocation(binding->gl_program, tmp);
    }

    for (int i = 0; i < NV2A_MAX_TEXTURES; i++) {
        snprintf(tmp, sizeof(tmp), "texScale%d", i);
        binding->tex_scale_loc[i] = glGetUniformLocation(binding->gl_program, tmp);
    }

    /* lookup vertex shader uniforms */
    for (int i = 0; i < NV2A_VERTEXSHADER_CONSTANTS; i++) {
        snprintf(tmp, sizeof(tmp), "c[%d]", i);
        binding->vsh_constant_loc[i] = glGetUniformLocation(binding->gl_program, tmp);
    }
    binding->surface_size_loc = glGetUniformLocation(binding->gl_program, "surfaceSize");
    binding->clip_range_loc = glGetUniformLocation(binding->gl_program, "clipRange");
    binding->fog_color_loc = glGetUniformLocation(binding->gl_program, "fogColor");
    binding->fog_param_loc = glGetUniformLocation(binding->gl_program, "fogParam");

    binding->inv_viewport_loc = glGetUniformLocation(binding->gl_program, "invViewport");
    for (int i = 0; i < NV2A_LTCTXA_COUNT; i++) {
        snprintf(tmp, sizeof(tmp), "ltctxa[%d]", i);
        binding->ltctxa_loc[i] = glGetUniformLocation(binding->gl_program, tmp);
    }
    for (int i = 0; i < NV2A_LTCTXB_COUNT; i++) {
        snprintf(tmp, sizeof(tmp), "ltctxb[%d]", i);
        binding->ltctxb_loc[i] = glGetUniformLocation(binding->gl_program, tmp);
    }
    for (int i = 0; i < NV2A_LTC1_COUNT; i++) {
        snprintf(tmp, sizeof(tmp), "ltc1[%d]", i);
        binding->ltc1_loc[i] = glGetUniformLocation(binding->gl_program, tmp);
    }
    for (int i = 0; i < NV2A_MAX_LIGHTS; i++) {
        snprintf(tmp, sizeof(tmp), "lightInfiniteHalfVector%d", i);
        binding->light_infinite_half_vector_loc[i] =
            glGetUniformLocation(binding->gl_program, tmp);
        snprintf(tmp, sizeof(tmp), "lightInfiniteDirection%d", i);
        binding->light_infinite_direction_loc[i] =
            glGetUniformLocation(binding->gl_program, tmp);

        snprintf(tmp, sizeof(tmp), "lightLocalPosition%d", i);
        binding->light_local_position_loc[i] = glGetUniformLocation(binding->gl_program, tmp);
        snprintf(tmp, sizeof(tmp), "lightLocalAttenuation%d", i);
        binding->light_local_attenuation_loc[i] =
            glGetUniformLocation(binding->gl_program, tmp);
    }
    for (int i = 0; i < 8; i++) {
        snprintf(tmp, sizeof(tmp), "clipRegion[%d]", i);
        binding->clip_region_loc[i] = glGetUniformLocation(binding->gl_program, tmp);
    }

    if (binding->state.fixed_function) {
        binding->material_alpha_loc =
            glGetUniformLocation(binding->gl_program, "material_alpha");
    } else {
        binding->material_alpha_loc = -1;
    }
}

static void generate_shaders(ShaderBinding *binding)
{
    char *previous_numeric_locale = setlocale(LC_NUMERIC, NULL);
    if (previous_numeric_locale) {
        previous_numeric_locale = g_strdup(previous_numeric_locale);
    }

    /* Ensure numeric values are printed with '.' radix, no grouping */
    setlocale(LC_NUMERIC, "C");
    GLuint program = glCreateProgram();

    ShaderState *state = &binding->state;

    /* Create an optional geometry shader and find primitive type */
    GLenum gl_primitive_mode =
        get_gl_primitive_mode(state->polygon_front_mode, state->primitive_mode);
    MString* geometry_shader_code =
        pgraph_gen_geom_glsl(state->polygon_front_mode,
                                 state->polygon_back_mode,
                                 state->primitive_mode,
                                 state->smooth_shading,
                                 false);
    if (geometry_shader_code) {
        const char* geometry_shader_code_str =
             mstring_get_str(geometry_shader_code);
        GLuint geometry_shader = create_gl_shader(GL_GEOMETRY_SHADER,
                                                  geometry_shader_code_str,
                                                  "geometry shader");
        glAttachShader(program, geometry_shader);
        mstring_unref(geometry_shader_code);
    }

    /* create the vertex shader */
    MString *vertex_shader_code =
        pgraph_gen_vsh_glsl(state, geometry_shader_code != NULL);
    GLuint vertex_shader = create_gl_shader(GL_VERTEX_SHADER,
                                            mstring_get_str(vertex_shader_code),
                                            "vertex shader");
    glAttachShader(program, vertex_shader);
    mstring_unref(vertex_shader_code);

    /* generate a fragment shader from register combiners */
    MString *fragment_shader_code = pgraph_gen_psh_glsl(state->psh);
    const char *fragment_shader_code_str =
        mstring_get_str(fragment_shader_code);
    GLuint fragment_shader = create_gl_shader(GL_FRAGMENT_SHADER,
                                              fragment_shader_code_str,
                                              "fragment shader");
    glAttachShader(program, fragment_shader);
    mstring_unref(fragment_shader_code);

    /* link the program */
    glLinkProgram(program);
    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if(!linked) {
        GLchar log[2048];
        glGetProgramInfoLog(program, 2048, NULL, log);
        fprintf(stderr, "nv2a: shader linking failed: %s\n", log);
        abort();
    }

    glUseProgram(program);

    binding->initialized = true;
    binding->gl_program = program;
    binding->gl_primitive_mode = gl_primitive_mode;
    update_shader_constant_locations(binding);

    if (previous_numeric_locale) {
        setlocale(LC_NUMERIC, previous_numeric_locale);
        g_free(previous_numeric_locale);
    }
}

static const char *shader_gl_vendor = NULL;

static void shader_create_cache_folder(void)
{
    char *shader_path = g_strdup_printf("%sshaders", xemu_settings_get_base_path());
    qemu_mkdir(shader_path);
    g_free(shader_path);
}

static char *shader_get_lru_cache_path(void)
{
    return g_strdup_printf("%s/shader_cache_list", xemu_settings_get_base_path());
}

static void shader_write_lru_list_entry_to_disk(Lru *lru, LruNode *node, void *opaque)
{
    FILE *lru_list_file = (FILE*) opaque;
    size_t written = fwrite(&node->hash, sizeof(uint64_t), 1, lru_list_file);
    if (written != 1) {
        fprintf(stderr, "nv2a: Failed to write shader list entry %llx to disk\n",
                (unsigned long long) node->hash);
    }
}

void pgraph_gl_shader_write_cache_reload_list(PGRAPHState *pg)
{
    PGRAPHGLState *r = pg->gl_renderer_state;

    if (!g_config.perf.cache_shaders) {
        qatomic_set(&r->shader_cache_writeback_pending, false);
        qemu_event_set(&r->shader_cache_writeback_complete);
        return;
    }

    char *shader_lru_path = shader_get_lru_cache_path();
    qemu_thread_join(&r->shader_disk_thread);

    FILE *lru_list = qemu_fopen(shader_lru_path, "wb");
    g_free(shader_lru_path);
    if (!lru_list) {
        fprintf(stderr, "nv2a: Failed to open shader LRU cache for writing\n");
        return;
    }

    lru_visit_active(&r->shader_cache, shader_write_lru_list_entry_to_disk, lru_list);
    fclose(lru_list);

    lru_flush(&r->shader_cache);

    qatomic_set(&r->shader_cache_writeback_pending, false);
    qemu_event_set(&r->shader_cache_writeback_complete);
}

bool pgraph_gl_shader_load_from_memory(ShaderBinding *binding)
{
    assert(glGetError() == GL_NO_ERROR);

    if (!binding->program) {
        return false;
    }

    GLuint gl_program = glCreateProgram();
    glProgramBinary(gl_program, binding->program_format, binding->program,
                    binding->program_size);
    GLint gl_error = glGetError();
    if (gl_error != GL_NO_ERROR) {
        NV2A_DPRINTF(
            "failed to load shader binary from disk: GL error code %d\n",
            gl_error);
        glDeleteProgram(gl_program);
        return false;
    }

    glValidateProgram(gl_program);
    GLint valid = 0;
    glGetProgramiv(gl_program, GL_VALIDATE_STATUS, &valid);
    if (!valid) {
        GLchar log[1024];
        glGetProgramInfoLog(gl_program, 1024, NULL, log);
        NV2A_DPRINTF("failed to load shader binary from disk: %s\n", log);
        glDeleteProgram(gl_program);
        return false;
    }

    glUseProgram(gl_program);

    binding->gl_program = gl_program;
    binding->gl_primitive_mode = get_gl_primitive_mode(
        binding->state.polygon_front_mode, binding->state.primitive_mode);
    binding->initialized = true;

    g_free(binding->program);
    binding->program = NULL;

    update_shader_constant_locations(binding);

    return true;
}

static char *shader_get_bin_directory(uint64_t hash)
{
    const char *cfg_dir = xemu_settings_get_base_path();
    char *shader_bin_dir =
        g_strdup_printf("%s/shaders/%04x", cfg_dir, (uint32_t)(hash >> 48));
    return shader_bin_dir;
}

static char *shader_get_binary_path(const char *shader_bin_dir, uint64_t hash)
{
    uint64_t bin_mask = (uint64_t)0xffff << 48;
    return g_strdup_printf("%s/%012" PRIx64, shader_bin_dir, hash & ~bin_mask);
}

static void shader_load_from_disk(PGRAPHState *pg, uint64_t hash)
{
    PGRAPHGLState *r = pg->gl_renderer_state;

    char *shader_bin_dir = shader_get_bin_directory(hash);
    char *shader_path = shader_get_binary_path(shader_bin_dir, hash);
    char *cached_xemu_version = NULL;
    char *cached_gl_vendor = NULL;
    void *program_buffer = NULL;

    uint64_t cached_xemu_version_len;
    uint64_t gl_vendor_len;
    GLenum program_binary_format;
    ShaderState state;
    size_t shader_size;

    g_free(shader_bin_dir);

    qemu_mutex_lock(&r->shader_cache_lock);
    if (lru_contains_hash(&r->shader_cache, hash)) {
        qemu_mutex_unlock(&r->shader_cache_lock);
        return;
    }
    qemu_mutex_unlock(&r->shader_cache_lock);

    FILE *shader_file = qemu_fopen(shader_path, "rb");
    if (!shader_file) {
        goto error;
    }

    size_t nread;
    #define READ_OR_ERR(data, data_len) \
        do { \
            nread = fread(data, data_len, 1, shader_file); \
            if (nread != 1) { \
                fclose(shader_file); \
                goto error; \
            } \
        } while (0)

    READ_OR_ERR(&cached_xemu_version_len, sizeof(cached_xemu_version_len));

    cached_xemu_version = g_malloc(cached_xemu_version_len +1);
    READ_OR_ERR(cached_xemu_version, cached_xemu_version_len);
    if (strcmp(cached_xemu_version, xemu_version) != 0) {
        fclose(shader_file);
        goto error;
    }

    READ_OR_ERR(&gl_vendor_len, sizeof(gl_vendor_len));

    cached_gl_vendor = g_malloc(gl_vendor_len);
    READ_OR_ERR(cached_gl_vendor, gl_vendor_len);
    if (strcmp(cached_gl_vendor, shader_gl_vendor) != 0) {
        fclose(shader_file);
        goto error;
    }

    READ_OR_ERR(&program_binary_format, sizeof(program_binary_format));
    READ_OR_ERR(&state, sizeof(state));
    READ_OR_ERR(&shader_size, sizeof(shader_size));

    program_buffer = g_malloc(shader_size);
    READ_OR_ERR(program_buffer, shader_size);

    #undef READ_OR_ERR

    fclose(shader_file);
    g_free(shader_path);
    g_free(cached_xemu_version);
    g_free(cached_gl_vendor);

    qemu_mutex_lock(&r->shader_cache_lock);
    LruNode *node = lru_lookup(&r->shader_cache, hash, &state);
    ShaderBinding *binding = container_of(node, ShaderBinding, node);

    /* If we happened to regenerate this shader already, then we may as well use the new one */
    if (binding->initialized) {
        qemu_mutex_unlock(&r->shader_cache_lock);
        return;
    }

    binding->program_format = program_binary_format;
    binding->program_size = shader_size;
    binding->program = program_buffer;
    binding->cached = true;
    qemu_mutex_unlock(&r->shader_cache_lock);
    return;

error:
    /* Delete the shader so it won't be loaded again */
    qemu_unlink(shader_path);
    g_free(shader_path);
    g_free(program_buffer);
    g_free(cached_xemu_version);
    g_free(cached_gl_vendor);
}

static void *shader_reload_lru_from_disk(void *arg)
{
    if (!g_config.perf.cache_shaders) {
        return NULL;
    }

    PGRAPHState *pg = (PGRAPHState*) arg;
    char *shader_lru_path = shader_get_lru_cache_path();

    FILE *lru_shaders_list = qemu_fopen(shader_lru_path, "rb");
    g_free(shader_lru_path);
    if (!lru_shaders_list) {
        return NULL;
    }

    uint64_t hash;
    while (fread(&hash, sizeof(uint64_t), 1, lru_shaders_list) == 1) {
        shader_load_from_disk(pg, hash);
    }

    return NULL;
}

static void shader_cache_entry_init(Lru *lru, LruNode *node, void *state)
{
    ShaderBinding *binding = container_of(node, ShaderBinding, node);
    memcpy(&binding->state, state, sizeof(ShaderState));
    binding->initialized = false;
    binding->cached = false;
    binding->program = NULL;
    binding->save_thread = NULL;
}

static void shader_cache_entry_post_evict(Lru *lru, LruNode *node)
{
    ShaderBinding *binding = container_of(node, ShaderBinding, node);

    if (binding->save_thread) {
        qemu_thread_join(binding->save_thread);
        g_free(binding->save_thread);
    }

    glDeleteProgram(binding->gl_program);
    if (binding->program) {
        g_free(binding->program);
    }

    binding->cached = false;
    binding->save_thread = NULL;
    binding->program = NULL;
    memset(&binding->state, 0, sizeof(ShaderState));
}

static bool shader_cache_entry_compare(Lru *lru, LruNode *node, void *key)
{
    ShaderBinding *binding = container_of(node, ShaderBinding, node);
    return memcmp(&binding->state, key, sizeof(ShaderState));
}

void pgraph_gl_init_shaders(PGRAPHState *pg)
{
    PGRAPHGLState *r = pg->gl_renderer_state;

    qemu_mutex_init(&r->shader_cache_lock);
    qemu_event_init(&r->shader_cache_writeback_complete, false);

    if (!shader_gl_vendor) {
        shader_gl_vendor = (const char *) glGetString(GL_VENDOR);
    }

    shader_create_cache_folder();

    /* FIXME: Make this configurable */
    const size_t shader_cache_size = 50*1024;
    lru_init(&r->shader_cache);
    r->shader_cache_entries = malloc(shader_cache_size * sizeof(ShaderBinding));
    assert(r->shader_cache_entries != NULL);
    for (int i = 0; i < shader_cache_size; i++) {
        lru_add_free(&r->shader_cache, &r->shader_cache_entries[i].node);
    }

    r->shader_cache.init_node = shader_cache_entry_init;
    r->shader_cache.compare_nodes = shader_cache_entry_compare;
    r->shader_cache.post_node_evict = shader_cache_entry_post_evict;

    qemu_thread_create(&r->shader_disk_thread, "pgraph.renderer_state->shader_cache",
                       shader_reload_lru_from_disk, pg, QEMU_THREAD_JOINABLE);
}

void pgraph_gl_finalize_shaders(PGRAPHState *pg)
{
    PGRAPHGLState *r = pg->gl_renderer_state;

    // Clear out shader cache
    pgraph_gl_shader_write_cache_reload_list(pg); // FIXME: also flushes, rename for clarity
    free(r->shader_cache_entries);
    r->shader_cache_entries = NULL;

    qemu_mutex_destroy(&r->shader_cache_lock);
}

static void *shader_write_to_disk(void *arg)
{
    ShaderBinding *binding = (ShaderBinding*) arg;

    char *shader_bin = shader_get_bin_directory(binding->node.hash);
    char *shader_path = shader_get_binary_path(shader_bin, binding->node.hash);

    static uint64_t gl_vendor_len;
    if (gl_vendor_len == 0) {
        gl_vendor_len = (uint64_t) (strlen(shader_gl_vendor) + 1);
    }

    static uint64_t xemu_version_len = 0;
    if (xemu_version_len == 0) {
        xemu_version_len = (uint64_t) (strlen(xemu_version) + 1);
    }

    qemu_mkdir(shader_bin);
    g_free(shader_bin);

    FILE *shader_file = qemu_fopen(shader_path, "wb");
    if (!shader_file) {
        goto error;
    }

    size_t written;
    #define WRITE_OR_ERR(data, data_size) \
        do { \
            written = fwrite(data, data_size, 1, shader_file); \
            if (written != 1) { \
                fclose(shader_file); \
                goto error; \
            } \
        } while (0)

    WRITE_OR_ERR(&xemu_version_len, sizeof(xemu_version_len));
    WRITE_OR_ERR(xemu_version, xemu_version_len);

    WRITE_OR_ERR(&gl_vendor_len, sizeof(gl_vendor_len));
    WRITE_OR_ERR(shader_gl_vendor, gl_vendor_len);

    WRITE_OR_ERR(&binding->program_format, sizeof(binding->program_format));
    WRITE_OR_ERR(&binding->state, sizeof(binding->state));

    WRITE_OR_ERR(&binding->program_size, sizeof(binding->program_size));
    WRITE_OR_ERR(binding->program, binding->program_size);

    #undef WRITE_OR_ERR

    fclose(shader_file);

    g_free(shader_path);
    g_free(binding->program);
    binding->program = NULL;

    return NULL;

error:
    fprintf(stderr, "nv2a: Failed to write shader binary file to %s\n", shader_path);
    qemu_unlink(shader_path);
    g_free(shader_path);
    g_free(binding->program);
    binding->program = NULL;
    return NULL;
}

void pgraph_gl_shader_cache_to_disk(ShaderBinding *binding)
{
    if (binding->cached) {
        return;
    }

    GLint program_size;
    glGetProgramiv(binding->gl_program, GL_PROGRAM_BINARY_LENGTH, &program_size);

    if (binding->program) {
        g_free(binding->program);
        binding->program = NULL;
    }

    /* program_size might be zero on some systems, if no binary formats are supported */
    if (program_size == 0) {
        return;
    }

    binding->program = g_malloc(program_size);
    GLsizei program_size_copied;
    glGetProgramBinary(binding->gl_program, program_size, &program_size_copied,
                       &binding->program_format, binding->program);
    assert(glGetError() == GL_NO_ERROR);

    binding->program_size = program_size_copied;
    binding->cached = true;

    char name[24];
    snprintf(name, sizeof(name), "scache-%llx", (unsigned long long) binding->node.hash);
    binding->save_thread = g_malloc0(sizeof(QemuThread));
    qemu_thread_create(binding->save_thread, name, shader_write_to_disk, binding, QEMU_THREAD_JOINABLE);
}

static void shader_update_constants(PGRAPHState *pg, ShaderBinding *binding,
                                    bool binding_changed)
{
    PGRAPHGLState *r = pg->gl_renderer_state;
    int i, j;

    /* update combiner constants */
    for (i = 0; i < 9; i++) {
        uint32_t constant[2];
        if (i == 8) {
            /* final combiner */
            constant[0] = pgraph_reg_r(pg, NV_PGRAPH_SPECFOGFACTOR0);
            constant[1] = pgraph_reg_r(pg, NV_PGRAPH_SPECFOGFACTOR1);
        } else {
            constant[0] = pgraph_reg_r(pg, NV_PGRAPH_COMBINEFACTOR0 + i * 4);
            constant[1] = pgraph_reg_r(pg, NV_PGRAPH_COMBINEFACTOR1 + i * 4);
        }

        for (j = 0; j < 2; j++) {
            GLint loc = binding->psh_constant_loc[i][j];
            if (loc != -1) {
                float value[4];
                pgraph_argb_pack32_to_rgba_float(constant[j], value);
                glUniform4fv(loc, 1, value);
            }
        }
    }
    if (binding->alpha_ref_loc != -1) {
        float alpha_ref = GET_MASK(pgraph_reg_r(pg, NV_PGRAPH_CONTROL_0),
                                   NV_PGRAPH_CONTROL_0_ALPHAREF) / 255.0;
        glUniform1f(binding->alpha_ref_loc, alpha_ref);
    }


    /* For each texture stage */
    for (i = 0; i < NV2A_MAX_TEXTURES; i++) {
        GLint loc;

        /* Bump luminance only during stages 1 - 3 */
        if (i > 0) {
            loc = binding->bump_mat_loc[i];
            if (loc != -1) {
                uint32_t m_u32[4];
                m_u32[0] = pgraph_reg_r(pg, NV_PGRAPH_BUMPMAT00 + 4 * (i - 1));
                m_u32[1] = pgraph_reg_r(pg, NV_PGRAPH_BUMPMAT01 + 4 * (i - 1));
                m_u32[2] = pgraph_reg_r(pg, NV_PGRAPH_BUMPMAT10 + 4 * (i - 1));
                m_u32[3] = pgraph_reg_r(pg, NV_PGRAPH_BUMPMAT11 + 4 * (i - 1));
                float m[4];
                m[0] = *(float*)&m_u32[0];
                m[1] = *(float*)&m_u32[1];
                m[2] = *(float*)&m_u32[2];
                m[3] = *(float*)&m_u32[3];
                glUniformMatrix2fv(loc, 1, GL_FALSE, m);
            }
            loc = binding->bump_scale_loc[i];
            if (loc != -1) {
                uint32_t v =
                    pgraph_reg_r(pg, NV_PGRAPH_BUMPSCALE1 + (i - 1) * 4);
                glUniform1f(loc, *(float*)&v);
            }
            loc = binding->bump_offset_loc[i];
            if (loc != -1) {
                uint32_t v =
                    pgraph_reg_r(pg, NV_PGRAPH_BUMPOFFSET1 + (i - 1) * 4);
                glUniform1f(loc, *(float*)&v);
            }
        }

        loc = r->shader_binding->tex_scale_loc[i];
        if (loc != -1) {
            assert(r->texture_binding[i] != NULL);
            glUniform1f(loc, (float)r->texture_binding[i]->scale);
        }
    }

    if (binding->fog_color_loc != -1) {
        uint32_t fog_color = pgraph_reg_r(pg, NV_PGRAPH_FOGCOLOR);
        glUniform4f(binding->fog_color_loc,
                    GET_MASK(fog_color, NV_PGRAPH_FOGCOLOR_RED) / 255.0,
                    GET_MASK(fog_color, NV_PGRAPH_FOGCOLOR_GREEN) / 255.0,
                    GET_MASK(fog_color, NV_PGRAPH_FOGCOLOR_BLUE) / 255.0,
                    GET_MASK(fog_color, NV_PGRAPH_FOGCOLOR_ALPHA) / 255.0);
    }
    if (binding->fog_param_loc != -1) {
        uint32_t v[2];
        v[0] = pgraph_reg_r(pg, NV_PGRAPH_FOGPARAM0);
        v[1] = pgraph_reg_r(pg, NV_PGRAPH_FOGPARAM1);
        glUniform2f(binding->fog_param_loc, *(float *)&v[0], *(float *)&v[1]);
    }

    float zmax;
    switch (pg->surface_shape.zeta_format) {
    case NV097_SET_SURFACE_FORMAT_ZETA_Z16:
        zmax = pg->surface_shape.z_format ? f16_max : (float)0xFFFF;
        break;
    case NV097_SET_SURFACE_FORMAT_ZETA_Z24S8:
        zmax = pg->surface_shape.z_format ? f24_max : (float)0xFFFFFF;
        break;
    default:
        assert(0);
    }

    if (binding->state.fixed_function) {
        /* update lighting constants */
        struct {
            uint32_t* v;
            bool* dirty;
            GLint* locs;
            size_t len;
        } lighting_arrays[] = {
            {&pg->ltctxa[0][0], &pg->ltctxa_dirty[0], binding->ltctxa_loc, NV2A_LTCTXA_COUNT},
            {&pg->ltctxb[0][0], &pg->ltctxb_dirty[0], binding->ltctxb_loc, NV2A_LTCTXB_COUNT},
            {&pg->ltc1[0][0], &pg->ltc1_dirty[0], binding->ltc1_loc, NV2A_LTC1_COUNT},
        };

        for (i=0; i<ARRAY_SIZE(lighting_arrays); i++) {
            uint32_t *lighting_v = lighting_arrays[i].v;
            bool *lighting_dirty = lighting_arrays[i].dirty;
            GLint *lighting_locs = lighting_arrays[i].locs;
            size_t lighting_len = lighting_arrays[i].len;
            for (j=0; j<lighting_len; j++) {
                if (!lighting_dirty[j] && !binding_changed) continue;
                GLint loc = lighting_locs[j];
                if (loc != -1) {
                    glUniform4fv(loc, 1, (const GLfloat*)&lighting_v[j*4]);
                }
                lighting_dirty[j] = false;
            }
        }

        for (i = 0; i < NV2A_MAX_LIGHTS; i++) {
            GLint loc;
            loc = binding->light_infinite_half_vector_loc[i];
            if (loc != -1) {
                glUniform3fv(loc, 1, pg->light_infinite_half_vector[i]);
            }
            loc = binding->light_infinite_direction_loc[i];
            if (loc != -1) {
                glUniform3fv(loc, 1, pg->light_infinite_direction[i]);
            }

            loc = binding->light_local_position_loc[i];
            if (loc != -1) {
                glUniform3fv(loc, 1, pg->light_local_position[i]);
            }
            loc = binding->light_local_attenuation_loc[i];
            if (loc != -1) {
                glUniform3fv(loc, 1, pg->light_local_attenuation[i]);
            }
        }

        /* estimate the viewport by assuming it matches the surface ... */
        unsigned int aa_width = 1, aa_height = 1;
        pgraph_apply_anti_aliasing_factor(pg, &aa_width, &aa_height);

        float m11 = 0.5 * (pg->surface_binding_dim.width/aa_width);
        float m22 = -0.5 * (pg->surface_binding_dim.height/aa_height);
        float m33 = zmax;
        float m41 = *(float*)&pg->vsh_constants[NV_IGRAPH_XF_XFCTX_VPOFF][0];
        float m42 = *(float*)&pg->vsh_constants[NV_IGRAPH_XF_XFCTX_VPOFF][1];

        float invViewport[16] = {
            1.0/m11, 0, 0, 0,
            0, 1.0/m22, 0, 0,
            0, 0, 1.0/m33, 0,
            -1.0+m41/m11, 1.0+m42/m22, 0, 1.0
        };

        if (binding->inv_viewport_loc != -1) {
            glUniformMatrix4fv(binding->inv_viewport_loc,
                               1, GL_FALSE, &invViewport[0]);
        }
    }

    /* update vertex program constants */
    for (i=0; i<NV2A_VERTEXSHADER_CONSTANTS; i++) {
        if (!pg->vsh_constants_dirty[i] && !binding_changed) continue;

        GLint loc = binding->vsh_constant_loc[i];
        if ((loc != -1) &&
            memcmp(binding->vsh_constants[i], pg->vsh_constants[i],
                   sizeof(pg->vsh_constants[1]))) {
            glUniform4fv(loc, 1, (const GLfloat *)pg->vsh_constants[i]);
            memcpy(binding->vsh_constants[i], pg->vsh_constants[i],
                   sizeof(pg->vsh_constants[i]));
        }

        pg->vsh_constants_dirty[i] = false;
    }

    if (binding->surface_size_loc != -1) {
        unsigned int aa_width = 1, aa_height = 1;
        pgraph_apply_anti_aliasing_factor(pg, &aa_width, &aa_height);
        glUniform2f(binding->surface_size_loc,
                    pg->surface_binding_dim.width / aa_width,
                    pg->surface_binding_dim.height / aa_height);
    }

    if (binding->clip_range_loc != -1) {
        uint32_t v[2];
        v[0] = pgraph_reg_r(pg, NV_PGRAPH_ZCLIPMIN);
        v[1] = pgraph_reg_r(pg, NV_PGRAPH_ZCLIPMAX);
        float zclip_min = *(float*)&v[0] / zmax * 2.0 - 1.0;
        float zclip_max = *(float*)&v[1] / zmax * 2.0 - 1.0;
        glUniform4f(binding->clip_range_loc, 0, zmax, zclip_min, zclip_max);
    }

    /* Clipping regions */
    unsigned int max_gl_width = pg->surface_binding_dim.width;
    unsigned int max_gl_height = pg->surface_binding_dim.height;
    pgraph_apply_scaling_factor(pg, &max_gl_width, &max_gl_height);

    for (i = 0; i < 8; i++) {
        uint32_t x = pgraph_reg_r(pg, NV_PGRAPH_WINDOWCLIPX0 + i * 4);
        unsigned int x_min = GET_MASK(x, NV_PGRAPH_WINDOWCLIPX0_XMIN);
        unsigned int x_max = GET_MASK(x, NV_PGRAPH_WINDOWCLIPX0_XMAX) + 1;
        uint32_t y = pgraph_reg_r(pg, NV_PGRAPH_WINDOWCLIPY0 + i * 4);
        unsigned int y_min = GET_MASK(y, NV_PGRAPH_WINDOWCLIPY0_YMIN);
        unsigned int y_max = GET_MASK(y, NV_PGRAPH_WINDOWCLIPY0_YMAX) + 1;
        pgraph_apply_anti_aliasing_factor(pg, &x_min, &y_min);
        pgraph_apply_anti_aliasing_factor(pg, &x_max, &y_max);

        pgraph_apply_scaling_factor(pg, &x_min, &y_min);
        pgraph_apply_scaling_factor(pg, &x_max, &y_max);

        /* Translate for the GL viewport origin */
        int y_min_xlat = MAX((int)max_gl_height - (int)y_max, 0);
        int y_max_xlat = MIN((int)max_gl_height - (int)y_min, max_gl_height);

        glUniform4i(r->shader_binding->clip_region_loc[i],
                    x_min, y_min_xlat, x_max, y_max_xlat);
    }

    if (binding->material_alpha_loc != -1) {
        glUniform1f(binding->material_alpha_loc, pg->material_alpha);
    }
}

static bool test_shaders_dirty(PGRAPHState *pg)
{
    #define CR_1(reg) CR_x(reg, 1)
    #define CR_4(reg) CR_x(reg, 4)
    #define CR_8(reg) CR_x(reg, 8)
    #define CF(src, name)  CF_x(typeof(src), (&src), name, 1)
    #define CFA(src, name) CF_x(typeof(src[0]), src, name, ARRAY_SIZE(src))
    #define CNAME(name) reg_check__ ## name
    #define CX_x__define(type, name, x) static type CNAME(name)[x];
    #define CR_x__define(reg, x) CX_x__define(uint32_t, reg, x)
    #define CF_x__define(type, src, name, x) CX_x__define(type, name, x)
    #define CR_x__check(reg, x) \
        for (int i = 0; i < x; i++) { if (pgraph_reg_r(pg, reg+i*4) != CNAME(reg)[i]) goto dirty; }
    #define CF_x__check(type, src, name, x) \
        for (int i = 0; i < x; i++) { if (src[i] != CNAME(name)[i]) goto dirty; }
    #define CR_x__update(reg, x) \
        for (int i = 0; i < x; i++) { CNAME(reg)[i] = pgraph_reg_r(pg, reg+i*4); }
    #define CF_x__update(type, src, name, x) \
        for (int i = 0; i < x; i++) { CNAME(name)[i] = src[i]; }

    #define DIRTY_REGS \
        CR_1(NV_PGRAPH_COMBINECTL) \
        CR_1(NV_PGRAPH_SHADERCTL) \
        CR_1(NV_PGRAPH_SHADOWCTL) \
        CR_1(NV_PGRAPH_COMBINESPECFOG0) \
        CR_1(NV_PGRAPH_COMBINESPECFOG1) \
        CR_1(NV_PGRAPH_CONTROL_0) \
        CR_1(NV_PGRAPH_CONTROL_3) \
        CR_1(NV_PGRAPH_CSV0_C) \
        CR_1(NV_PGRAPH_CSV0_D) \
        CR_1(NV_PGRAPH_CSV1_A) \
        CR_1(NV_PGRAPH_CSV1_B) \
        CR_1(NV_PGRAPH_SETUPRASTER) \
        CR_1(NV_PGRAPH_SHADERPROG) \
        CR_8(NV_PGRAPH_COMBINECOLORI0) \
        CR_8(NV_PGRAPH_COMBINECOLORO0) \
        CR_8(NV_PGRAPH_COMBINEALPHAI0) \
        CR_8(NV_PGRAPH_COMBINEALPHAO0) \
        CR_8(NV_PGRAPH_COMBINEFACTOR0) \
        CR_8(NV_PGRAPH_COMBINEFACTOR1) \
        CR_1(NV_PGRAPH_SHADERCLIPMODE) \
        CR_4(NV_PGRAPH_TEXCTL0_0) \
        CR_4(NV_PGRAPH_TEXFMT0) \
        CR_4(NV_PGRAPH_TEXFILTER0) \
        CR_8(NV_PGRAPH_WINDOWCLIPX0) \
        CR_8(NV_PGRAPH_WINDOWCLIPY0) \
        CF(pg->primitive_mode, primitive_mode) \
        CF(pg->surface_scale_factor, surface_scale_factor) \
        CF(pg->compressed_attrs, compressed_attrs) \
        CFA(pg->texture_matrix_enable, texture_matrix_enable)

    #define CR_x(reg, x) CR_x__define(reg, x)
    #define CF_x(type, src, name, x) CF_x__define(type, src, name, x)
    DIRTY_REGS
    #undef CR_x
    #undef CF_x

    #define CR_x(reg, x) CR_x__check(reg, x)
    #define CF_x(type, src, name, x) CF_x__check(type, src, name, x)
    DIRTY_REGS
    #undef CR_x
    #undef CF_x
    return false;

dirty:
    #define CR_x(reg, x) CR_x__update(reg, x)
    #define CF_x(type, src, name, x) CF_x__update(type, src, name, x)
    DIRTY_REGS
    #undef CR_x
    #undef CF_x
    return true;
}

void pgraph_gl_bind_shaders(PGRAPHState *pg)
{
    PGRAPHGLState *r = pg->gl_renderer_state;

    bool binding_changed = false;
    if (r->shader_binding && !test_shaders_dirty(pg) && !pg->program_data_dirty) {
        nv2a_profile_inc_counter(NV2A_PROF_SHADER_BIND_NOTDIRTY);
        goto update_constants;
    }

    ShaderBinding *old_binding = r->shader_binding;
    ShaderState state = pgraph_get_shader_state(pg);
    assert(!state.vulkan);

    NV2A_GL_DGROUP_BEGIN("%s (VP: %s FFP: %s)", __func__,
                         state.vertex_program ? "yes" : "no",
                         state.fixed_function ? "yes" : "no");

    qemu_mutex_lock(&r->shader_cache_lock);

    uint64_t shader_state_hash =
        fast_hash((uint8_t *)&state, sizeof(ShaderState));

    LruNode *node = lru_lookup(&r->shader_cache, shader_state_hash, &state);
    ShaderBinding *binding = container_of(node, ShaderBinding, node);

    if (!binding->initialized && !pgraph_gl_shader_load_from_memory(binding)) {
        nv2a_profile_inc_counter(NV2A_PROF_SHADER_GEN);
        generate_shaders(binding);
        if (g_config.perf.cache_shaders) {
            pgraph_gl_shader_cache_to_disk(binding);
        }
    }
    assert(binding->initialized);
    r->shader_binding = binding;
    pg->program_data_dirty = false;

    qemu_mutex_unlock(&r->shader_cache_lock);

    binding_changed = (r->shader_binding != old_binding);
    if (binding_changed) {
        nv2a_profile_inc_counter(NV2A_PROF_SHADER_BIND);
        glUseProgram(r->shader_binding->gl_program);
    }

    NV2A_GL_DGROUP_END();

update_constants:
    assert(r->shader_binding);
    assert(r->shader_binding->initialized);
    shader_update_constants(pg, r->shader_binding, binding_changed);
}

GLuint pgraph_gl_compile_shader(const char *vs_src, const char *fs_src)
{
    GLint status;
    char err_buf[512];

    // Compile vertex shader
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vs_src, NULL);
    glCompileShader(vs);
    glGetShaderiv(vs, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        glGetShaderInfoLog(vs, sizeof(err_buf), NULL, err_buf);
        err_buf[sizeof(err_buf)-1] = '\0';
        fprintf(stderr, "Vertex shader compilation failed: %s\n", err_buf);
        exit(1);
    }

    // Compile fragment shader
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fs_src, NULL);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        glGetShaderInfoLog(fs, sizeof(err_buf), NULL, err_buf);
        err_buf[sizeof(err_buf)-1] = '\0';
        fprintf(stderr, "Fragment shader compilation failed: %s\n", err_buf);
        exit(1);
    }

    // Link vertex and fragment shaders
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glUseProgram(prog);

    // Flag shaders for deletion (will still be retained for lifetime of prog)
    glDeleteShader(vs);
    glDeleteShader(fs);

    return prog;
}
