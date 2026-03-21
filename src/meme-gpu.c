

#include "meme-gpu.h"
#include <epoxy/gl.h>
#include <gdk/gdk.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

GdkPixbuf *meme_apply_saturation_contrast(GdkPixbuf *src, double sat, double contrast);
GdkPixbuf *meme_apply_deep_fry(GdkPixbuf *src);

typedef struct {
    gboolean initialised;
    gboolean available;
    GdkGLContext *ctx;

    GLuint prog_sat_contrast; 
    GLuint prog_deep_fry;
    GLuint prog_passthrough;
    GLuint prog_composite;

    GLuint vao;
    GLuint vbo;
} GpuState;

static GpuState g_gpu = {FALSE, FALSE, NULL, 0, 0, 0, 0, 0};


// Shader bodies have NO #version header — we prepend the right one at
// runtime after realizing the GL context and checking its actual type.
// GLES path  = "#version 300 es\nprecision mediump float;\n"
// (GLSL ES 3.00 is the minimum GLES 3.0 version; universally supported)
// Desktop GL = "#version 130\n"
// (GLSL 1.30 introduced layout qualifiers, texture(), out fragColor)
// Both versions support every feature used below.


static const char *VERT_BODY =
    "layout(location = 0) in vec2 aPos;\n"
    "layout(location = 1) in vec2 aTex;\n"
    "out vec2 vTex;\n"
    "void main() {\n"
    "  gl_Position = vec4(aPos, 0.0, 1.0);\n"
    "  vTex = aTex;\n"
    "}\n";

static const char *SAT_CONTRAST_BODY =
    "in  vec2      vTex;\n"
    "out vec4      fragColor;\n"
    "uniform sampler2D uTex;\n"
    "uniform float     uSat;\n"
    "uniform float     uContrast;\n"
    "void main() {\n"
    "  vec4  c    = texture(uTex, vTex);\n"
    "  float gray = dot(c.rgb, vec3(0.2990, 0.5870, 0.1140));\n"
    "  vec3  sat  = mix(vec3(gray), c.rgb, uSat);\n"
    "  vec3  con  = (sat - 0.5) * uContrast + 0.5;\n"
    "  fragColor  = vec4(clamp(con, 0.0, 1.0), c.a);\n"
    "}\n";

static const char *DEEP_FRY_BODY =
    "in  vec2      vTex;\n"
    "out vec4      fragColor;\n"
    "uniform sampler2D uTex;\n"
    "uniform float     uSeed;\n"
    "uniform float     uBlockSize;\n"
    "\n"
    "float hash(vec2 p) {\n"
    "  p = fract(p * vec2(127.1, 311.7) + uSeed);\n"
    "  p += dot(p, p.yx + 19.19);\n"
    "  return fract((p.x + p.y) * p.x) * 2.0 - 1.0;\n"
    "}\n"
    "\n"
    "void main() {\n"
    "  vec2 snapped = floor(vTex / uBlockSize) * uBlockSize;\n"
    "  vec4 c = texture(uTex, snapped);\n"
    "  float n = hash(vTex) * 0.118;\n"
    "  c.rgb += vec3(n);\n"
    "  c.rgb = (c.rgb - 0.5) * 2.0 + 0.5;\n"
    "  fragColor = vec4(clamp(c.rgb, 0.0, 1.0), c.a);\n"
    "}\n";

static const char *PASSTHROUGH_BODY =
    "in  vec2      vTex;\n"
    "out vec4      fragColor;\n"
    "uniform sampler2D uTex;\n"
    "void main() { fragColor = texture(uTex, vTex); }\n";

static const char *COMPOSITE_BODY =
    "in  vec2 vTex;\n"
    "out vec4 fragColor;\n"
    "uniform sampler2D uBg;\n"
    "uniform sampler2D uLayer;\n"
    "uniform vec2  uCenter;\n" 
    "uniform float uRotCos;\n"
    "uniform float uRotSin;\n" 
    "uniform float uScale;\n"
    "uniform float uLayerW;\n"
    "uniform float uLayerH;\n" 
    "uniform float uImgW;\n"
    "uniform float uImgH;\n"
    "uniform float uOpacity;\n"
    "uniform int   uBlendMode;\n" 
    "\n"
    "void main() {\n"
    "  vec4 bg = texture(uBg, vec2(vTex.x, 1.0 - vTex.y));\n"
    "  vec2 p = (vTex - uCenter) * vec2(uImgW, uImgH);\n"
    "  vec2 pr = vec2(p.x*uRotCos - p.y*uRotSin,\n"
    "                 p.x*uRotSin + p.y*uRotCos);\n"
    "  vec2 luv = pr / (uScale * vec2(uLayerW, uLayerH)) + 0.5;\n"
    "  if (any(lessThan(luv, vec2(0.0))) ||\n"
    "      any(greaterThan(luv, vec2(1.0)))) { fragColor = bg; return; }\n"
    "\n"
    "  vec4 lay = texture(uLayer, luv);\n"
    "  lay.a *= uOpacity;\n"
    "\n"
    "  vec3 blended;\n"
    "  if (uBlendMode == 1) {\n"
    "    blended = bg.rgb * lay.rgb;\n"
    "  } else if (uBlendMode == 2) {\n"
    "    blended = 1.0 - (1.0-bg.rgb)*(1.0-lay.rgb);\n"
    "  } else if (uBlendMode == 3) {\n" 
    "    vec3 drk = 2.0*bg.rgb*lay.rgb;\n"
    "    vec3 lgt = 1.0-2.0*(1.0-bg.rgb)*(1.0-lay.rgb);\n"
    "    blended = mix(drk, lgt, step(0.5, bg.rgb));\n"
    "  } else {\n" 
    "    blended = lay.rgb;\n"
    "  }\n"
    "\n"
    "  fragColor = vec4(mix(bg.rgb, blended, lay.a),\n"
    "                   bg.a + lay.a*(1.0-bg.a));\n"
    "}\n";


static char *
make_shader_src(gboolean use_es, const char *body) {
    const char *hdr = use_es
                          ? "#version 300 es\nprecision mediump float;\n"
                          : "#version 130\n";
    return g_strconcat(hdr, body, NULL);
}


static GLuint
compile_shader(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);

    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[512];
        glGetShaderInfoLog(s, sizeof buf, NULL, buf);
        g_warning("meme-gpu: shader compile error: %s", buf);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static GLuint
link_program(const char *vert_src, const char *frag_src) {
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vert_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, frag_src);
    if (!vs || !fs) {
        if (vs)
            glDeleteShader(vs);
        if (fs)
            glDeleteShader(fs);
        return 0;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[512];
        glGetProgramInfoLog(prog, sizeof buf, NULL, buf);
        g_warning("meme-gpu: program link error: %s", buf);
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

//Upload a GdkPixbuf (RGB or RGBA) to a new GL texture.
//Returns the texture name, or 0 on error.
//GdkPixbuf is always packed (rowstride == width * n_channels for standard
//pixbufs, but we handle the non-tight case by uploading row-by-row).
static GLuint pixbuf_to_texture (GdkPixbuf *pb) {
    int       w          = gdk_pixbuf_get_width        (pb);
    int       h          = gdk_pixbuf_get_height       (pb);
    int       n_ch       = gdk_pixbuf_get_n_channels   (pb);
    int       rowstride  = gdk_pixbuf_get_rowstride    (pb);
    const guchar *pixels = gdk_pixbuf_get_pixels       (pb);   
    GLenum    fmt        = (n_ch == 4) ? GL_RGBA : GL_RGB;
    GLenum    int_fmt    = (n_ch == 4) ? GL_RGBA8 : GL_RGB8;    
    GLuint tex = 0;
    
    glGenTextures (1, &tex);
    glBindTexture (GL_TEXTURE_2D, tex);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);   
    
    glPixelStorei (GL_UNPACK_ALIGNMENT, 1);

    if (rowstride == w * n_ch) {
        glTexImage2D (GL_TEXTURE_2D, 0, int_fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, pixels);
    } else {
        glTexImage2D (GL_TEXTURE_2D, 0, int_fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, NULL);
        for (int row = 0; row < h; row++) {
            glTexSubImage2D (GL_TEXTURE_2D, 0,
                     0, row, w, 1,
                     fmt, GL_UNSIGNED_BYTE,
                     pixels + row * rowstride);
        }
    }   
    
    glPixelStorei (GL_UNPACK_ALIGNMENT, 4);
    glBindTexture (GL_TEXTURE_2D, 0);
    
    return tex;
}


static void
draw_quad(int w, int h) {
    glViewport(0, 0, w, h);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindVertexArray(g_gpu.vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

static GdkPixbuf *
readback_to_pixbuf(int w, int h) {
    guchar *buf = g_malloc(w * h * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, buf);

    GdkPixbuf *out = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, w, h);
    int out_rs = gdk_pixbuf_get_rowstride(out);
    guchar *out_px = gdk_pixbuf_get_pixels(out);

    for (int y = 0; y < h; y++) {
        const guchar *src_row = buf + (h - 1 - y) * w * 4;
        guchar *dst_row = out_px + y * out_rs;
        memcpy(dst_row, src_row, w * 4);
    }

    g_free(buf);
    return out;
}

 
static GLuint
make_fbo(int w, int h, GLuint *tex_out) {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, tex, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        g_warning("meme-gpu: FBO incomplete (status=0x%x)", status);
        glDeleteFramebuffers(1, &fbo);
        glDeleteTextures(1, &tex);
        return 0;
    }

    *tex_out = tex;
    return fbo;
}


gboolean
meme_gpu_init(GdkDisplay *display) {
    if (g_gpu.initialised)
        return g_gpu.available;

    g_gpu.initialised = TRUE;
    g_gpu.available = FALSE;

    if (!display)
        display = gdk_display_get_default();

    GError *err = NULL;
    g_gpu.ctx = gdk_display_create_gl_context(display, &err);
    if (!g_gpu.ctx) {
        g_message("meme-gpu: cannot create GL context (%s) — using CPU path",
                  err ? err->message : "unknown");
        g_clear_error(&err);
        return FALSE;
    }

    if (!gdk_gl_context_realize(g_gpu.ctx, &err)) {
        g_message("meme-gpu: cannot realize GL context (%s) — using CPU path",
                  err ? err->message : "unknown");
        g_clear_error(&err);
        g_clear_object(&g_gpu.ctx);
        return FALSE;
    }

    gdk_gl_context_make_current(g_gpu.ctx);

    gboolean use_es = gdk_gl_context_get_use_es(g_gpu.ctx);
    g_message("meme-gpu: context type: %s", use_es ? "GLES" : "desktop GL");

    char *vert_src = make_shader_src(use_es, VERT_BODY);
    char *sc_src = make_shader_src(use_es, SAT_CONTRAST_BODY);
    char *df_src = make_shader_src(use_es, DEEP_FRY_BODY);
    char *pt_src = make_shader_src(use_es, PASSTHROUGH_BODY);

    g_gpu.prog_sat_contrast = link_program(vert_src, sc_src);
    g_gpu.prog_deep_fry = link_program(vert_src, df_src);
    g_gpu.prog_passthrough = link_program(vert_src, pt_src);

    char *comp_src = make_shader_src(use_es, COMPOSITE_BODY);
    g_gpu.prog_composite = link_program(vert_src, comp_src);
    g_free(comp_src);

    g_free(vert_src);
    g_free(sc_src);
    g_free(df_src);
    g_free(pt_src);

    if (!g_gpu.prog_sat_contrast || !g_gpu.prog_deep_fry ||
        !g_gpu.prog_passthrough || !g_gpu.prog_composite) {
        g_warning("meme-gpu: shader compilation failed — using CPU path");
        gdk_gl_context_clear_current();
        return FALSE;
    }
    static const GLfloat QUAD[] = {
        -1.0f,
        1.0f,
        0.0f,
        0.0f,
        -1.0f,
        -1.0f,
        0.0f,
        1.0f,
        1.0f,
        1.0f,
        1.0f,
        0.0f,
        1.0f,
        -1.0f,
        1.0f,
        1.0f,
    };

    glGenVertexArrays(1, &g_gpu.vao);
    glGenBuffers(1, &g_gpu.vbo);
    glBindVertexArray(g_gpu.vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_gpu.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof QUAD, QUAD, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(GLfloat), (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(GLfloat),
                          (void *)(2 * sizeof(GLfloat)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    gdk_gl_context_clear_current();

    g_gpu.available = TRUE;
    g_message("meme-gpu: GPU acceleration active (GL %s)",
              glGetString(GL_VERSION));
    return TRUE;
}

void meme_gpu_cleanup(void) {
    if (!g_gpu.available)
        return;

    gdk_gl_context_make_current(g_gpu.ctx);

    if (g_gpu.prog_sat_contrast)
        glDeleteProgram(g_gpu.prog_sat_contrast);
    if (g_gpu.prog_deep_fry)
        glDeleteProgram(g_gpu.prog_deep_fry);
    if (g_gpu.prog_passthrough)
        glDeleteProgram(g_gpu.prog_passthrough);
    if (g_gpu.prog_composite)
        glDeleteProgram(g_gpu.prog_composite);
    if (g_gpu.vao)
        glDeleteVertexArrays(1, &g_gpu.vao);
    if (g_gpu.vbo)
        glDeleteBuffers(1, &g_gpu.vbo);

    gdk_gl_context_clear_current();
    g_clear_object(&g_gpu.ctx);

    memset(&g_gpu, 0, sizeof g_gpu);
}

GdkPixbuf *
meme_gpu_apply_saturation_contrast(GdkPixbuf *src, double sat, double contrast) {
    if (fabs(sat - 1.0) < 1e-6 && fabs(contrast - 1.0) < 1e-6) {
        g_object_ref(src);
        return src;
    }

    if (!g_gpu.available)
        return meme_apply_saturation_contrast(src, sat, contrast);

    int w = gdk_pixbuf_get_width(src);
    int h = gdk_pixbuf_get_height(src);

    gdk_gl_context_make_current(g_gpu.ctx);

    GLuint src_tex = pixbuf_to_texture(src);
    GLuint fbo_tex = 0;
    GLuint fbo = make_fbo(w, h, &fbo_tex);

    if (!fbo) {
        glDeleteTextures(1, &src_tex);
        gdk_gl_context_clear_current();
        return meme_apply_saturation_contrast(src, sat, contrast);
    }

    glUseProgram(g_gpu.prog_sat_contrast);
    glUniform1i(glGetUniformLocation(g_gpu.prog_sat_contrast, "uTex"), 0);
    glUniform1f(glGetUniformLocation(g_gpu.prog_sat_contrast, "uSat"), (GLfloat)sat);
    glUniform1f(glGetUniformLocation(g_gpu.prog_sat_contrast, "uContrast"), (GLfloat)contrast);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, src_tex);

    draw_quad(w, h);

    GdkPixbuf *out = readback_to_pixbuf(w, h);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &fbo_tex);
    glDeleteTextures(1, &src_tex);
    glUseProgram(0);

    gdk_gl_context_clear_current();
    return out;
}

GdkPixbuf *
meme_gpu_apply_deep_fry(GdkPixbuf *src) {
    if (!g_gpu.available)
        return meme_apply_deep_fry(src);

    int w = gdk_pixbuf_get_width(src);
    int h = gdk_pixbuf_get_height(src);

    gdk_gl_context_make_current(g_gpu.ctx);

    GLuint src_tex = pixbuf_to_texture(src);
    GLuint fbo_tex = 0;
    GLuint fbo = make_fbo(w, h, &fbo_tex);

    if (!fbo) {
        glDeleteTextures(1, &src_tex);
        gdk_gl_context_clear_current();
        return meme_apply_deep_fry(src);
    }

    static float seed_counter = 0.0f;
    seed_counter += 0.137f;
    if (seed_counter > 1000.0f)
        seed_counter = 0.0f;

    float block_size = 4.0f / (float)w;

    glUseProgram(g_gpu.prog_deep_fry);
    glUniform1i(glGetUniformLocation(g_gpu.prog_deep_fry, "uTex"), 0);
    glUniform1f(glGetUniformLocation(g_gpu.prog_deep_fry, "uSeed"), seed_counter);
    glUniform1f(glGetUniformLocation(g_gpu.prog_deep_fry, "uBlockSize"), block_size);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, src_tex);

    draw_quad(w, h);

    GdkPixbuf *out = readback_to_pixbuf(w, h);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &fbo_tex);
    glDeleteTextures(1, &src_tex);
    glUseProgram(0);

    gdk_gl_context_clear_current();
    return out;
}

GdkPixbuf *
meme_gpu_apply_effects(GdkPixbuf *composite, gboolean cinematic, gboolean deep_fry) {
    if (!cinematic && !deep_fry) {
        g_object_ref(composite);
        return composite;
    }

    GdkPixbuf *result = composite;
    gboolean own = FALSE;

    if (cinematic) {
        GdkPixbuf *tmp = meme_gpu_apply_saturation_contrast(result, 1.15, 1.05);
        if (own)
            g_object_unref(result);
        result = tmp;
        own = TRUE;
    }

    if (deep_fry) {
        GdkPixbuf *tmp = meme_gpu_apply_deep_fry(result);
        if (own)
            g_object_unref(result);
        result = tmp;
        own = TRUE;
    }

    if (!own)
        g_object_ref(result);

    return result;
}

gboolean
meme_gpu_is_available(void) {
    return g_gpu.available;
}


GdkPixbuf *
meme_gpu_composite_layers(GdkPixbuf *bg, MemeGpuLayer *layers, int n_layers) {
    if (!g_gpu.available || !bg)
        return NULL;

    int w = gdk_pixbuf_get_width(bg);
    int h = gdk_pixbuf_get_height(bg);

    gdk_gl_context_make_current(g_gpu.ctx);

    GLuint tex_a = 0, tex_b = 0;
    GLuint fbo_a = make_fbo(w, h, &tex_a);
    GLuint fbo_b = make_fbo(w, h, &tex_b);

    if (!fbo_a || !fbo_b) {
        if (fbo_a) {
            glDeleteFramebuffers(1, &fbo_a);
            glDeleteTextures(1, &tex_a);
        }
        if (fbo_b) {
            glDeleteFramebuffers(1, &fbo_b);
            glDeleteTextures(1, &tex_b);
        }
        gdk_gl_context_clear_current();
        return NULL;
    }

    GLuint bg_tex = pixbuf_to_texture(bg);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_a);
    glUseProgram(g_gpu.prog_passthrough);
    glUniform1i(glGetUniformLocation(g_gpu.prog_passthrough, "uTex"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, bg_tex);
    draw_quad(w, h);
    glDeleteTextures(1, &bg_tex);

    GLuint cur_fbo = fbo_a, cur_tex = tex_a;
    GLuint nxt_fbo = fbo_b, nxt_tex = tex_b;

    glUseProgram(g_gpu.prog_composite);
    GLint loc_bg = glGetUniformLocation(g_gpu.prog_composite, "uBg");
    GLint loc_layer = glGetUniformLocation(g_gpu.prog_composite, "uLayer");
    GLint loc_cx = glGetUniformLocation(g_gpu.prog_composite, "uCenter");
    GLint loc_rc = glGetUniformLocation(g_gpu.prog_composite, "uRotCos");
    GLint loc_rs = glGetUniformLocation(g_gpu.prog_composite, "uRotSin");
    GLint loc_sc = glGetUniformLocation(g_gpu.prog_composite, "uScale");
    GLint loc_lw = glGetUniformLocation(g_gpu.prog_composite, "uLayerW");
    GLint loc_lh = glGetUniformLocation(g_gpu.prog_composite, "uLayerH");
    GLint loc_iw = glGetUniformLocation(g_gpu.prog_composite, "uImgW");
    GLint loc_ih = glGetUniformLocation(g_gpu.prog_composite, "uImgH");
    GLint loc_op = glGetUniformLocation(g_gpu.prog_composite, "uOpacity");
    GLint loc_bm = glGetUniformLocation(g_gpu.prog_composite, "uBlendMode");

    for (int i = 0; i < n_layers; i++) {
        MemeGpuLayer *ly = &layers[i];
        if (!ly->pixbuf)
            continue;

        GLuint layer_tex = pixbuf_to_texture(ly->pixbuf);
        int lw = gdk_pixbuf_get_width(ly->pixbuf);
        int lh = gdk_pixbuf_get_height(ly->pixbuf);

        glBindFramebuffer(GL_FRAMEBUFFER, nxt_fbo);

        glUniform1i(loc_bg, 1);
        glUniform1i(loc_layer, 0);
        glUniform2f(loc_cx, (GLfloat)ly->x, (GLfloat)ly->y);
        glUniform1f(loc_rc, (GLfloat)cos(-ly->rotation));
        glUniform1f(loc_rs, (GLfloat)sin(-ly->rotation));
        glUniform1f(loc_sc, (GLfloat)ly->scale);
        glUniform1f(loc_lw, (GLfloat)lw);
        glUniform1f(loc_lh, (GLfloat)lh);
        glUniform1f(loc_iw, (GLfloat)w);
        glUniform1f(loc_ih, (GLfloat)h);
        glUniform1f(loc_op, (GLfloat)ly->opacity);
        glUniform1i(loc_bm, ly->blend_mode);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, layer_tex);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, cur_tex);

        draw_quad(w, h);

        glDeleteTextures(1, &layer_tex);

        GLuint tmp_fbo = cur_fbo, tmp_tex = cur_tex;
        cur_fbo = nxt_fbo;
        cur_tex = nxt_tex;
        nxt_fbo = tmp_fbo;
        nxt_tex = tmp_tex;
    }

    glUseProgram(0);

    glBindFramebuffer(GL_FRAMEBUFFER, cur_fbo);
    GdkPixbuf *out = readback_to_pixbuf(w, h);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo_a);
    glDeleteTextures(1, &tex_a);
    glDeleteFramebuffers(1, &fbo_b);
    glDeleteTextures(1, &tex_b);

    gdk_gl_context_clear_current();
    return out;
}