#include "musdesk/render/VisualizerRenderer.hpp"
#include "musdesk/core/TimeFormat.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <stdexcept>
#include <type_traits>

namespace musdesk {
namespace {
struct GlFunctions {
    PFNGLGENFRAMEBUFFERSPROC GenFramebuffers{};
    PFNGLDELETEFRAMEBUFFERSPROC DeleteFramebuffers{};
    PFNGLBINDFRAMEBUFFERPROC BindFramebuffer{};
    PFNGLFRAMEBUFFERTEXTURE2DPROC FramebufferTexture2D{};
    PFNGLCHECKFRAMEBUFFERSTATUSPROC CheckFramebufferStatus{};
    PFNGLGENVERTEXARRAYSPROC GenVertexArrays{};
    PFNGLDELETEVERTEXARRAYSPROC DeleteVertexArrays{};
    PFNGLBINDVERTEXARRAYPROC BindVertexArray{};
    PFNGLCREATESHADERPROC CreateShader{};
    PFNGLSHADERSOURCEPROC ShaderSource{};
    PFNGLCOMPILESHADERPROC CompileShader{};
    PFNGLGETSHADERIVPROC GetShaderiv{};
    PFNGLGETSHADERINFOLOGPROC GetShaderInfoLog{};
    PFNGLDELETESHADERPROC DeleteShader{};
    PFNGLCREATEPROGRAMPROC CreateProgram{};
    PFNGLATTACHSHADERPROC AttachShader{};
    PFNGLLINKPROGRAMPROC LinkProgram{};
    PFNGLGETPROGRAMIVPROC GetProgramiv{};
    PFNGLGETPROGRAMINFOLOGPROC GetProgramInfoLog{};
    PFNGLDELETEPROGRAMPROC DeleteProgram{};
    PFNGLUSEPROGRAMPROC UseProgram{};
    PFNGLGETUNIFORMLOCATIONPROC GetUniformLocation{};
    PFNGLUNIFORM1IPROC Uniform1i{};
    PFNGLUNIFORM1FPROC Uniform1f{};
    PFNGLUNIFORM1FVPROC Uniform1fv{};
    PFNGLUNIFORM2FPROC Uniform2f{};
    PFNGLUNIFORM4FPROC Uniform4f{};
    PFNGLACTIVETEXTUREPROC ActiveTexture{};
    PFNGLGENBUFFERSPROC GenBuffers{};
    PFNGLDELETEBUFFERSPROC DeleteBuffers{};
    PFNGLBINDBUFFERPROC BindBuffer{};
    PFNGLBUFFERDATAPROC BufferData{};
    PFNGLENABLEVERTEXATTRIBARRAYPROC EnableVertexAttribArray{};
    PFNGLVERTEXATTRIBPOINTERPROC VertexAttribPointer{};
    PFNGLBLENDFUNCSEPARATEPROC BlendFuncSeparate{};

    void load() {
        auto get = [](auto& function, const char* name) {
            function = reinterpret_cast<std::remove_reference_t<decltype(function)>>(SDL_GL_GetProcAddress(name));
            if (function == nullptr) {
                throw std::runtime_error(std::string("Missing OpenGL function: ") + name);
            }
        };
#define LOAD_GL(name) get(name, "gl" #name)
        LOAD_GL(GenFramebuffers); LOAD_GL(DeleteFramebuffers); LOAD_GL(BindFramebuffer);
        LOAD_GL(FramebufferTexture2D); LOAD_GL(CheckFramebufferStatus);
        LOAD_GL(GenVertexArrays); LOAD_GL(DeleteVertexArrays); LOAD_GL(BindVertexArray);
        LOAD_GL(CreateShader); LOAD_GL(ShaderSource); LOAD_GL(CompileShader); LOAD_GL(GetShaderiv);
        LOAD_GL(GetShaderInfoLog); LOAD_GL(DeleteShader); LOAD_GL(CreateProgram); LOAD_GL(AttachShader);
        LOAD_GL(LinkProgram); LOAD_GL(GetProgramiv); LOAD_GL(GetProgramInfoLog); LOAD_GL(DeleteProgram);
        LOAD_GL(UseProgram); LOAD_GL(GetUniformLocation); LOAD_GL(Uniform1i); LOAD_GL(Uniform1f); LOAD_GL(Uniform1fv);
        LOAD_GL(Uniform2f); LOAD_GL(Uniform4f); LOAD_GL(ActiveTexture); LOAD_GL(GenBuffers);
        LOAD_GL(DeleteBuffers); LOAD_GL(BindBuffer); LOAD_GL(BufferData);
        LOAD_GL(EnableVertexAttribArray); LOAD_GL(VertexAttribPointer); LOAD_GL(BlendFuncSeparate);
#undef LOAD_GL
    }
};

constexpr const char* sceneVertex = R"glsl(#version 330 core
out vec2 uv;
void main() {
    vec2 corner = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    uv = corner;
    gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);
}
)glsl";

constexpr const char* sceneFragment = R"glsl(#version 330 core
in vec2 uv;
out vec4 fragColor;
uniform sampler2D artwork;
uniform sampler2D backgroundImage;
uniform bool hasArtwork;
uniform float discAngle;
uniform float backgroundAngle;
uniform float backgroundZoom;
uniform vec2 backgroundOffset;
uniform float progress;
uniform float audioEnergy;
uniform float subjectScale;
uniform float ringMotion;
uniform float ringPhase;
uniform float spectrum[32];
uniform float spectrumStrength;
uniform float spectrumLevel;
uniform float ringHeadroom;
uniform vec2 canvasSize;
uniform vec4 discGeometry;
uniform vec4 timelineGeometry;
uniform vec4 backgroundCrop;
uniform vec4 labelCrop;
uniform float backgroundSoftness;
uniform vec2 cameraPose;
uniform vec4 glowSettings;
uniform float pulseStrength;
uniform float backgroundBrightness;

vec2 rotatePoint(vec2 p, float angle) {
    float c = cos(angle), s = sin(angle);
    return vec2(c*p.x-s*p.y, s*p.x+c*p.y);
}
vec2 cropPoint(vec2 p, vec4 crop) {
    return (p-0.5)*crop.xy+crop.zw;
}
float segmentDistance(vec2 p, vec2 a, vec2 b) {
    vec2 ab = b-a;
    return length(p-a-ab*clamp(dot(p-a,ab)/max(dot(ab,ab),0.0001),0.0,1.0));
}
float ringHash(float sector, float moment) {
    return fract(sin(dot(vec2(sector,moment),vec2(127.1,311.7)))*43758.5453);
}
float ringNoise(float angle, float time) {
    float position = (angle+3.141592654)/6.283185307*12.0;
    vec2 cell = floor(vec2(position,time));
    vec2 blend = smoothstep(vec2(0.0),vec2(1.0),fract(vec2(position,time)));
    float left = mod(cell.x,12.0), right = mod(cell.x+1.0,12.0);
    return mix(mix(ringHash(left,cell.y),ringHash(right,cell.y),blend.x),
        mix(ringHash(left,cell.y+1.0),ringHash(right,cell.y+1.0),blend.x),blend.y);
}
vec2 spectrumVertex(float node) {
    node = mod(node,128.0);
    float angle = node/128.0*6.283185307-3.141592654;
    float irregularity = ringNoise(angle,ringPhase*1.3);
    float bandPosition = acos(clamp(cos(4.0*angle+1.8*(irregularity-0.5)
        +ringPhase*0.35),-1.0,1.0))/3.141592654*31.0;
    int band = min(int(bandPosition),30);
    float spectral = mix(spectrum[band],spectrum[band+1],fract(bandPosition));
    float moment = ringPhase*10.0;
    float motion = mix(ringHash(node,floor(moment)),ringHash(node,floor(moment)+1.0),
        smoothstep(0.0,1.0,fract(moment)));
    float level = spectrumLevel*spectrumLevel;
    spectral = (0.22*level+0.78*spectral*spectral)*(0.8+0.2*irregularity);
    float wave = 0.5+0.3*sin((angle-discAngle)*18.0+ringPhase)
        +0.2*sin((angle-discAngle)*31.0-ringPhase*1.3);
    float radius = 1.055+0.025*ringMotion*wave
        +ringHeadroom*(1.0-exp(-spectrumStrength*spectral))*(0.25+0.75*motion);
    return vec2(cos(angle),sin(angle))*radius;
}
void main() {
    vec2 topUv = vec2(uv.x, 1.0-uv.y);
    topUv = (topUv-0.5-vec2(cameraPose.x,0.0))/cameraPose.y+0.5;
    vec2 p = topUv * canvasSize;
    vec3 color = mix(vec3(0.025,0.04,0.075), vec3(0.085,0.13,0.20), 1.0-topUv.y);
    if (hasArtwork) {
        float aspect = canvasSize.x/canvasSize.y;
        vec2 bg = (topUv-0.5)*vec2(aspect,1.0);
        bg = rotatePoint(bg,backgroundAngle)/backgroundZoom;
        bg = bg/vec2(aspect,1.0) + 0.5 + backgroundOffset;
        vec2 imageUv = cropPoint(bg,backgroundCrop);
        vec4 sampleColor;
        if (backgroundSoftness <= 0.0) sampleColor = texture(artwork,imageUv);
        else if (backgroundSoftness >= 1.0) sampleColor = texture(backgroundImage,imageUv);
        else sampleColor = mix(texture(artwork,imageUv),texture(backgroundImage,imageUv),backgroundSoftness);
        vec3 lifted = pow(max(sampleColor.rgb,vec3(0.0)),vec3(0.72));
        color = mix(color,lifted,sampleColor.a);
    }
    color *= backgroundBrightness;
    float vignette = 1.0 - 0.55*smoothstep(0.15,0.80,length(topUv-0.5));
    color *= vignette * (1.0-0.26*smoothstep(0.5,0.9,topUv.y));

    vec2 discPoint = (p-discGeometry.xy)/(discGeometry.z*subjectScale);
    float r = length(discPoint);
    vec2 material = rotatePoint(discPoint,-discAngle);
    float phi = atan(material.y,material.x);
    vec3 accent = glowSettings.rgb;
    float pulse = audioEnergy*pulseStrength;
    float wave = 0.0;
    if (ringMotion > 0.0) wave = 0.5 + 0.3*sin(phi*18.0+ringPhase) + 0.2*sin(phi*31.0-ringPhase*1.3);
    float glowDistance = max(0.0,r-0.90);
    color += accent * glowSettings.a * (1.0+1.3*pulse)
        * (0.5*exp(-glowDistance*glowDistance*(38.0-10.0*pulse)));
    float rimRadius = 1.055 + 0.025*ringMotion*wave;
    float rimDistance = abs(r-rimRadius);
    if (spectrumStrength > 0.0) {
        rimDistance = 100.0;
        if (r > 1.0 && r < 1.12+ringHeadroom) {
            float node = floor((atan(discPoint.y,discPoint.x)+3.141592654)/6.283185307*128.0);
            rimDistance = segmentDistance(discPoint,spectrumVertex(node),spectrumVertex(node+1.0));
        }
    }
    float lineStrength = min(1.0,spectrumStrength+ringMotion);
    color += accent * lineStrength * (0.9*exp(-rimDistance*230.0)+0.12*exp(-rimDistance*65.0));
    if (r < 1.0) {
        float grooves = 0.009*sin(r*950.0) + 0.006*sin(r*420.0);
        float reflection = pow(abs(cos(phi-0.5)),22.0)*0.10;
        vec3 vinyl = vec3(0.015,0.021,0.029) + grooves + reflection;
        vinyl += 0.018*pow(abs(sin(phi+0.6)),10.0);
        if (r < 0.60) {
            vec2 labelUv = material/1.20+0.5;
            vec4 label = hasArtwork ? texture(artwork,cropPoint(labelUv,labelCrop)) : vec4(0.10,0.17,0.25,1.0);
            vinyl = mix(vec3(0.06,0.08,0.12),label.rgb,label.a);
        }
        if (r < 0.040) {
            vinyl = mix(vec3(0.04,0.06,0.09),
                        vec3(0.65,0.75,0.86),
                        1.0-smoothstep(0.0,0.040,r));
        }
        color = mix(color,vinyl,1.0-smoothstep(0.994,1.0,r));
    }
    vec2 barStart = vec2(timelineGeometry.x,timelineGeometry.z);
    vec2 barEnd = vec2(timelineGeometry.y,timelineGeometry.z);
    vec2 overlayPoint = discGeometry.xy+(p-discGeometry.xy)/subjectScale;
    float bar = segmentDistance(overlayPoint,barStart,barEnd);
    color = mix(color,vec3(0.20,0.26,0.36),1.0-smoothstep(3.0,5.0,bar));
    vec2 marker = mix(barStart,barEnd,progress);
    float fill = segmentDistance(overlayPoint,barStart,marker);
    color += accent*0.18*exp(-fill*0.12);
    color = mix(color,vec3(0.65,0.86,1.0),1.0-smoothstep(3.0,5.0,fill));
    color = mix(color,vec3(0.84,0.94,1.0),1.0-smoothstep(11.0,13.0,length(overlayPoint-marker)));
    fragColor = vec4(color,1.0);
}
)glsl";

constexpr const char* textVertex = R"glsl(#version 330 core
layout(location=0) in vec2 position;
layout(location=1) in vec2 texCoord;
out vec2 uv;
uniform vec2 canvasSize;
uniform vec2 cameraPose;
void main() {
    vec2 point = (position/canvasSize-0.5)*cameraPose.y+0.5+vec2(cameraPose.x,0.0);
    gl_Position=vec4(point.x*2.0-1.0,1.0-point.y*2.0,0.0,1.0);
    uv=texCoord;
}
)glsl";
constexpr const char* textFragment = R"glsl(#version 330 core
in vec2 uv;
out vec4 fragColor;
uniform sampler2D fontAtlas;
uniform vec4 textColor;
void main() { fragColor=vec4(textColor.rgb,textColor.a*texture(fontAtlas,uv).r); }
)glsl";

GLuint compileShader(GlFunctions& gl, GLenum type, const char* source) {
    const GLuint shader = gl.CreateShader(type);
    gl.ShaderSource(shader, 1, &source, nullptr);
    gl.CompileShader(shader);
    GLint success = 0;
    gl.GetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        std::array<char, 2048> message{};
        gl.GetShaderInfoLog(shader, static_cast<GLsizei>(message.size()), nullptr, message.data());
        gl.DeleteShader(shader);
        throw std::runtime_error(std::string("Shader compilation failed: ") + message.data());
    }
    return shader;
}

GLuint createProgram(GlFunctions& gl, const char* vertexSource, const char* fragmentSource) {
    GLuint vertex = 0, fragment = 0, program = 0;
    try {
        vertex = compileShader(gl, GL_VERTEX_SHADER, vertexSource);
        fragment = compileShader(gl, GL_FRAGMENT_SHADER, fragmentSource);
        program = gl.CreateProgram();
        gl.AttachShader(program, vertex);
        gl.AttachShader(program, fragment);
        gl.LinkProgram(program);
        GLint success = 0;
        gl.GetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            std::array<char, 2048> message{};
            gl.GetProgramInfoLog(program, static_cast<GLsizei>(message.size()), nullptr, message.data());
            throw std::runtime_error(std::string("Shader link failed: ") + message.data());
        }
    } catch (...) {
        if (vertex) gl.DeleteShader(vertex);
        if (fragment) gl.DeleteShader(fragment);
        if (program) gl.DeleteProgram(program);
        throw;
    }
    gl.DeleteShader(vertex);
    gl.DeleteShader(fragment);
    return program;
}

GLuint uploadTexture(int width, int height, const void* pixels, bool singleChannel = false) {
    GLint maxSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
    if (width > maxSize || height > maxSize) {
        throw std::runtime_error("Image exceeds the graphics driver's maximum texture size.");
    }
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, singleChannel ? GL_R8 : GL_RGBA8, width, height, 0,
        singleChannel ? GL_RED : GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    if (glGetError() != GL_NO_ERROR) {
        glDeleteTextures(1, &texture);
        throw std::runtime_error("Cannot allocate image texture on the graphics device.");
    }
    return texture;
}
}

struct VisualizerRenderer::Impl {
    GlFunctions gl;
    GLuint framebuffer = 0, canvas = 0, artwork = 0, background = 0, fontAtlas = 0;
    GLuint sceneProgram = 0, textProgram = 0, sceneVao = 0, textVao = 0, textBuffer = 0;
    float imageAspect = 1.0f;
    int pixelsWidth = width, pixelsHeight = height;
    std::array<stbtt_packedchar, 352> glyphs{};

    ~Impl() {
        // Zero handles also make partially initialized construction exception-safe.
        if (textBuffer) gl.DeleteBuffers(1, &textBuffer);
        if (sceneVao) gl.DeleteVertexArrays(1, &sceneVao);
        if (textVao) gl.DeleteVertexArrays(1, &textVao);
        if (sceneProgram) gl.DeleteProgram(sceneProgram);
        if (textProgram) gl.DeleteProgram(textProgram);
        if (framebuffer) gl.DeleteFramebuffers(1, &framebuffer);
        const GLuint textures[]{canvas, artwork, background, fontAtlas};
        glDeleteTextures(4, textures);
    }

    void initialize(const std::string& fontPath) {
        gl.load();
        sceneProgram = createProgram(gl, sceneVertex, sceneFragment);
        textProgram = createProgram(gl, textVertex, textFragment);
        gl.GenVertexArrays(1, &sceneVao);
        gl.GenVertexArrays(1, &textVao);
        gl.GenBuffers(1, &textBuffer);
        gl.BindVertexArray(textVao);
        gl.BindBuffer(GL_ARRAY_BUFFER, textBuffer);
        gl.EnableVertexAttribArray(0);
        gl.EnableVertexAttribArray(1);
        gl.VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
        gl.VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
        gl.BindVertexArray(0);
        gl.BindBuffer(GL_ARRAY_BUFFER, 0);

        canvas = uploadTexture(width, height, nullptr);
        gl.GenFramebuffers(1, &framebuffer);
        gl.BindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, canvas, 0);
        const bool complete = gl.CheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
        gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
        if (!complete) throw std::runtime_error("Cannot create the composition framebuffer.");

        std::size_t fontBytes = 0;
        std::unique_ptr<void, decltype(&SDL_free)> font(SDL_LoadFile(fontPath.c_str(), &fontBytes), SDL_free);
        if (!font || fontBytes == 0) throw std::runtime_error("Cannot read the bundled Roboto font.");
        std::vector<unsigned char> bitmap(2048 * 1024);
        stbtt_pack_context pack{};
        if (!stbtt_PackBegin(&pack, bitmap.data(), 2048, 1024, 0, 1, nullptr)) {
            throw std::runtime_error("Cannot allocate the font atlas.");
        }
        const int packed = stbtt_PackFontRange(&pack, static_cast<const unsigned char*>(font.get()),
            0, 80.0f, 32, static_cast<int>(glyphs.size()), glyphs.data());
        stbtt_PackEnd(&pack);
        if (!packed) throw std::runtime_error("Cannot pack the composition font.");
        fontAtlas = uploadTexture(2048, 1024, bitmap.data(), true);
        gl.UseProgram(textProgram);
        gl.Uniform1i(gl.GetUniformLocation(textProgram, "fontAtlas"), 0);
        gl.UseProgram(0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    static unsigned int nextGlyph(const char*& cursor) {
        const Uint32 codepoint = SDL_StepUTF8(&cursor, nullptr);
        return codepoint >= 32 && codepoint < 384 ? codepoint - 32 : '?' - 32;
    }

    float textWidth(const std::string& text) const {
        float result = 0;
        const char* cursor = text.c_str();
        while (*cursor) result += glyphs[nextGlyph(cursor)].xadvance;
        return result;
    }

    void text(const std::string& value, float x, float baseline, float scale,
              const std::array<float, 4>& color) {
        std::vector<float> vertices;
        float penX = 0, penY = 0;
        const char* cursor = value.c_str();
        while (*cursor) {
            stbtt_aligned_quad quad{};
            stbtt_GetPackedQuad(glyphs.data(), 2048, 1024, static_cast<int>(nextGlyph(cursor)),
                &penX, &penY, &quad, 0);
            const float left = x + quad.x0 * scale, right = x + quad.x1 * scale;
            const float top = baseline + quad.y0 * scale, bottom = baseline + quad.y1 * scale;
            const float triangle[]{left,top,quad.s0,quad.t0, right,top,quad.s1,quad.t0,
                right,bottom,quad.s1,quad.t1, left,top,quad.s0,quad.t0,
                right,bottom,quad.s1,quad.t1, left,bottom,quad.s0,quad.t1};
            vertices.insert(vertices.end(), std::begin(triangle), std::end(triangle));
        }
        gl.Uniform4f(gl.GetUniformLocation(textProgram, "textColor"), color[0], color[1], color[2], color[3]);
        gl.BufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
            vertices.data(), GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size() / 4));
    }

    void centeredText(const std::string& value, float baseline, float scale, const std::array<float, 4>& color,
        float centerX, float maximumWidth) {
        const float unscaledWidth = textWidth(value);
        if (unscaledWidth > 0) scale = std::min(scale, maximumWidth / unscaledWidth);
        const float x = centerX - unscaledWidth * scale * 0.5f;
        text(value, x + 2, baseline + 3, scale, {0, 0, 0, 0.6f});
        text(value, x, baseline, scale, color);
    }
};

VisualizerRenderer::VisualizerRenderer(const std::string& fontPath) : impl_(std::make_unique<Impl>()) {
    impl_->initialize(fontPath);
}
VisualizerRenderer::~VisualizerRenderer() = default;

void VisualizerRenderer::setArtwork(const ImageData& image) {
    const auto blurred = blurredBackground(image);
    GLuint newArtwork = 0, newBackground = 0;
    try {
        newArtwork = uploadTexture(image.width, image.height, image.rgba.data());
        newBackground = uploadTexture(blurred.width, blurred.height, blurred.rgba.data());
    } catch (...) {
        glDeleteTextures(1, &newArtwork);
        glDeleteTextures(1, &newBackground);
        throw;
    }
    glDeleteTextures(1, &impl_->artwork);
    glDeleteTextures(1, &impl_->background);
    impl_->artwork = newArtwork;
    impl_->background = newBackground;
    impl_->imageAspect = static_cast<float>(image.width) / image.height;
}

void VisualizerRenderer::clearArtwork() {
    glDeleteTextures(1, &impl_->artwork);
    glDeleteTextures(1, &impl_->background);
    impl_->artwork = impl_->background = 0;
}

void VisualizerRenderer::renderFrame(double timeSeconds, const CompositionSettings& settings, float audioEnergy) {
    renderFrame(timeSeconds, settings, AudioReaction{audioEnergy, 0, 0});
}

void VisualizerRenderer::renderFrame(double timeSeconds, const CompositionSettings& settings, const AudioReaction& audio) {
    renderAtSize(timeSeconds,settings,audio,outputSize(settings.format,settings.shortEdge));
}

void VisualizerRenderer::renderPreviewFrame(double timeSeconds, const CompositionSettings& settings,
    const AudioReaction& audio, int displayedShortEdge) {
    renderAtSize(timeSeconds,settings,audio,previewSize(settings.format,displayedShortEdge,settings.shortEdge));
}

void VisualizerRenderer::renderAtSize(double timeSeconds, const CompositionSettings& settings,
    const AudioReaction& audio, OutputSize size) {
    auto& gl = impl_->gl;
    auto layout = layoutFor(settings.format);
    if (size.width != impl_->pixelsWidth || size.height != impl_->pixelsHeight) {
        GLint maximum = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximum);
        if (size.width > maximum || size.height > maximum) throw std::runtime_error("Output exceeds the GPU texture limit.");
        glBindTexture(GL_TEXTURE_2D, impl_->canvas);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size.width, size.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        if (glGetError() != GL_NO_ERROR) throw std::runtime_error("Cannot resize the composition framebuffer.");
        impl_->pixelsWidth = size.width;
        impl_->pixelsHeight = size.height;
    }
    const auto frame = compositionAt(timeSeconds, settings, audio);
    GLint oldFramebuffer = 0;
    std::array<GLint, 4> oldViewport{};
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldFramebuffer);
    glGetIntegerv(GL_VIEWPORT, oldViewport.data());
    gl.BindFramebuffer(GL_FRAMEBUFFER, impl_->framebuffer);
    glViewport(0, 0, size.width, size.height);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    gl.UseProgram(impl_->sceneProgram);
    gl.Uniform2f(gl.GetUniformLocation(impl_->sceneProgram, "canvasSize"),
        static_cast<float>(layout.canvas.width), static_cast<float>(layout.canvas.height));
    gl.Uniform4f(gl.GetUniformLocation(impl_->sceneProgram, "discGeometry"), layout.discX, layout.discY, layout.discRadius, 0);
    gl.Uniform4f(gl.GetUniformLocation(impl_->sceneProgram, "timelineGeometry"),
        layout.timelineLeft, layout.timelineRight, layout.timelineY, 0);
    auto scalar = [&](const char* name, double value) {
        gl.Uniform1f(gl.GetUniformLocation(impl_->sceneProgram, name), static_cast<float>(value));
    };
    gl.ActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, impl_->artwork);
    gl.Uniform1i(gl.GetUniformLocation(impl_->sceneProgram, "artwork"), 0);
    gl.ActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, impl_->background);
    gl.Uniform1i(gl.GetUniformLocation(impl_->sceneProgram, "backgroundImage"), 1);
    gl.Uniform1i(gl.GetUniformLocation(impl_->sceneProgram, "hasArtwork"), impl_->artwork != 0);
    const auto setCrop = [&](const char* name, const ImageCrop& crop) {
        gl.Uniform4f(gl.GetUniformLocation(impl_->sceneProgram, name),
            static_cast<float>(crop.scaleX), static_cast<float>(crop.scaleY),
            static_cast<float>(crop.centerX), static_cast<float>(crop.centerY));
    };
    setCrop("backgroundCrop", imageCrop(impl_->imageAspect,
        static_cast<double>(layout.canvas.width) / layout.canvas.height, settings.backgroundFraming));
    setCrop("labelCrop", imageCrop(impl_->imageAspect, 1, settings.labelFraming));
    scalar("discAngle", frame.discAngleRadians);
    scalar("backgroundAngle", frame.backgroundAngleRadians);
    scalar("backgroundZoom", frame.backgroundZoom);
    scalar("progress", frame.progress);
    scalar("audioEnergy", std::isfinite(audio.energy) ? std::clamp(audio.energy, 0.0f, 1.0f) : 0);
    scalar("subjectScale", frame.subjectScale);
    gl.Uniform2f(gl.GetUniformLocation(impl_->sceneProgram, "cameraPose"),
        static_cast<float>(frame.cameraOffsetX), static_cast<float>(frame.cameraZoom));
    scalar("ringMotion", frame.ringMotion);
    scalar("ringPhase", frame.timeSeconds * 0.8);
    const auto& preset = settings.preset;
    auto safe = [](float value, float minimum, float maximum, float fallback) {
        return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
    };
    gl.Uniform4f(gl.GetUniformLocation(impl_->sceneProgram, "glowSettings"),
        safe(preset.glowColor[0], 0, 1, 0.24f), safe(preset.glowColor[1], 0, 1, 0.6f),
        safe(preset.glowColor[2], 0, 1, 1), preset.glowEnabled ? safe(preset.glowIntensity, 0, 3, 0.8f) : 0);
    scalar("pulseStrength", safe(preset.pulseStrength, 0, 2, 0.6f));
    scalar("spectrumStrength", preset.spectrumEnabled
        ? safe(preset.spectrumStrength, 0, 4, 0) * safe(preset.pulseStrength, 0, 2, 0) : 0);
    const double edgeDistance = std::min({layout.discX,layout.discY,
        layout.canvas.width-layout.discX,layout.canvas.height-layout.discY});
    const auto topGlyph = std::min_element(impl_->glyphs.begin(),impl_->glyphs.end(),
        [](const auto& a, const auto& b) { return a.yoff < b.yoff; });
    const double textDistance = (layout.textX == layout.discX
        ? layout.titleY-layout.discY+topGlyph->yoff
        : layout.textX-layout.textWidth/2-layout.discX)*frame.subjectScale;
    scalar("ringHeadroom",std::max(0.0,(std::min(edgeDistance,textDistance)-12)/(layout.discRadius*frame.subjectScale)-1.105));
    const auto spectrum = spectrumForDisplay(audio.spectrum, preset.spectrumGainDb, preset.spectrumBalance);
    scalar("spectrumLevel",*std::max_element(spectrum.begin(),spectrum.end()));
    gl.Uniform1fv(gl.GetUniformLocation(impl_->sceneProgram, "spectrum[0]"), static_cast<GLsizei>(spectrum.size()), spectrum.data());
    scalar("backgroundBrightness", safe(preset.backgroundBrightness, 0, 1.5f, 0.88f));
    scalar("backgroundSoftness", safe(settings.backgroundSoftness, 0, 1, 1));
    gl.Uniform2f(gl.GetUniformLocation(impl_->sceneProgram, "backgroundOffset"),
        static_cast<float>(frame.backgroundOffsetX), static_cast<float>(frame.backgroundOffsetY));
    gl.BindVertexArray(impl_->sceneVao);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    // Text uses its own VAO and a dynamic vertex buffer.
    glEnable(GL_BLEND);
    gl.BlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    gl.UseProgram(impl_->textProgram);
    gl.Uniform2f(gl.GetUniformLocation(impl_->textProgram, "canvasSize"),
        static_cast<float>(layout.canvas.width), static_cast<float>(layout.canvas.height));
    gl.Uniform2f(gl.GetUniformLocation(impl_->textProgram, "cameraPose"),
        static_cast<float>(frame.cameraOffsetX), static_cast<float>(frame.cameraZoom));
    gl.ActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, impl_->fontAtlas);
    gl.BindVertexArray(impl_->textVao);
    gl.BindBuffer(GL_ARRAY_BUFFER, impl_->textBuffer);
    const auto subjectScale = static_cast<float>(frame.subjectScale);
    const auto overlayX = [&](float x) { return layout.discX + (x-layout.discX)*subjectScale; };
    const auto overlayY = [&](float y) { return layout.discY + (y-layout.discY)*subjectScale; };
    impl_->centeredText(settings.title.empty() ? "Track title" : settings.title,
        layout.discY + (layout.titleY-layout.discY) * subjectScale, subjectScale, {0.92f,0.95f,1,1},
        overlayX(layout.textX), layout.textWidth * subjectScale);
    impl_->centeredText(settings.artist.empty() ? "Artist name" : settings.artist,
        layout.discY + (layout.artistY-layout.discY) * subjectScale, 0.64f * subjectScale, {0.60f,0.68f,0.80f,1},
        overlayX(layout.textX), layout.textWidth * subjectScale);
    const double duration = std::isfinite(settings.durationSeconds) && settings.durationSeconds > 0
        ? settings.durationSeconds : 252.0;
    const auto positionLabel = formatTime(static_cast<std::uint64_t>(std::min(frame.timeSeconds, 1.0e12)));
    const auto durationLabel = formatTime(static_cast<std::uint64_t>(std::min(duration, 1.0e12)));
    impl_->text(positionLabel, overlayX(layout.timelineLeft), overlayY(layout.timelineY + 60),
        0.54f * subjectScale, {0.66f,0.73f,0.84f,1});
    impl_->text(durationLabel, overlayX(layout.timelineRight - impl_->textWidth(durationLabel) * 0.54f),
        overlayY(layout.timelineY + 60), 0.54f * subjectScale, {0.66f,0.73f,0.84f,1});
    gl.BindVertexArray(0);
    gl.BindBuffer(GL_ARRAY_BUFFER, 0);
    gl.UseProgram(0);
    gl.BindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(oldFramebuffer));
    glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);
}

std::uint32_t VisualizerRenderer::textureId() const { return impl_->canvas; }
int VisualizerRenderer::outputWidth() const { return impl_->pixelsWidth; }
int VisualizerRenderer::outputHeight() const { return impl_->pixelsHeight; }

std::vector<std::uint8_t> VisualizerRenderer::readPixels() const {
    GLint oldFramebuffer = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldFramebuffer);
    impl_->gl.BindFramebuffer(GL_FRAMEBUFFER, impl_->framebuffer);
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(impl_->pixelsWidth) * impl_->pixelsHeight * 4);
    glReadPixels(0, 0, impl_->pixelsWidth, impl_->pixelsHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    impl_->gl.BindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(oldFramebuffer));
    return pixels;
}
}
