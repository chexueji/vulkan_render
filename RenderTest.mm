#include <memory>
#include "VRVulkan.h"
#include "core/VulkanRuntime.h"

#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <QuartzCore/QuartzCore.h>

#include <iostream>
#include <fstream>

using namespace VR;
using namespace VR::backend;

// call backs
void errorCallback(int error, const char *description)
{
    VR_PRINT("GLFW Error (code %d): %s\n", error, description);
}

void windowCloseCallback(GLFWwindow *window)
{
    glfwSetWindowShouldClose(window, GLFW_TRUE);
}

void windowResizeCallback(GLFWwindow *window, int width, int height)
{
    // TODO
}

void keyInputCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

void cursorPositionCallback(GLFWwindow *window, double xpos, double ypos)
{
    // VR_PRINT("Cursor Position X:%f Y:%f\n", xpos, ypos);
}

void mouseButtonCallback(GLFWwindow *window, int button, int action, int)
{
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    // VR_PRINT("Mouse Action: %d\n", action);
}

std::vector<uint8_t> readShaderBinary(const std::string filename)
{
    std::vector<uint8_t> data;
    std::ifstream file;
    
    std::string filepath = "shaders/spirvs/" + filename;

    file.open(filepath, std::ios::in | std::ios::binary);

    if (!file.is_open())
    {
        VR_PRINT("Failed to open file:%s\n", filename.c_str());
        return std::vector<uint8_t>();
    }

    file.seekg(0, std::ios::end);
    uint64_t read_count = static_cast<uint64_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    data.resize(static_cast<size_t>(read_count));
    file.read(reinterpret_cast<char *>(data.data()), read_count);
    file.close();

    return data;
}

typedef struct Vec2 {
    float x;
    float y;
    Vec2(float _x, float _y):x(_x), y(_y) {}
} Vec2;

typedef struct Vec3 {
    float x;
    float y;
    float z;
    Vec3(float _x, float _y, float _z):x(_x), y(_y), z(_z) {}
} Vec3;

uint32_t getTextureMipmapLevels(uint32_t texWidth, uint32_t texHeight)
{
    uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;
    return mipLevels;
}

#define FRAME_WIDTH 512
#define FRAME_HEIGHT 512

int main(int argc, const char* argv[]) {

    if (!glfwInit())
    {
        return -1;
    }
 
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(FRAME_WIDTH, FRAME_HEIGHT, "Vulkan Render Test", NULL, NULL);

    if (!window) {
        glfwTerminate();
        return -1;
    }
    // windows callbacks
    glfwMakeContextCurrent(window);
    glfwSetErrorCallback(errorCallback);
    glfwSetWindowCloseCallback(window, windowCloseCallback);
    glfwSetWindowSizeCallback(window, windowResizeCallback);
    glfwSetKeyCallback(window, keyInputCallback);
    glfwSetCursorPosCallback(window, cursorPositionCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);

    glfwSetInputMode(window, GLFW_STICKY_KEYS, 1);
    glfwSetInputMode(window, GLFW_STICKY_MOUSE_BUTTONS, 1);
    
    // request a Metal layer for rendering via MoltenVK
    NSWindow *win = glfwGetCocoaWindow(window);
    NSView* view = [win contentView];
    [view setWantsLayer:YES];
    CAMetalLayer* metalLayer = [CAMetalLayer layer];
    metalLayer.bounds = view.bounds;
    int w = view.bounds.size.width;
    int h = view.bounds.size.height;
    metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm; // FIXME: should be MTLPixelFormatRGBA8Unorm
    metalLayer.drawableSize = [view convertSizeToBacking:view.bounds.size];
    metalLayer.contentsScale = view.window.backingScaleFactor;
    metalLayer.opaque = YES;
    [view setLayer:metalLayer];

    std::unique_ptr<VulkanSurface> surface(new VulkanSurface);
    std::vector<const char *> requiredInstanceExtensions = { "VK_KHR_surface", };
#if defined(VR_VULKAN_VALIDATION)
    std::vector<const char *> requiredValidationLayers = {"VK_LAYER_KHRONOS_validation", };
#else
    std::vector<const char *> requiredValidationLayers = {};
#endif

#if defined(__APPLE__) || defined(__MACOSX__)
    requiredInstanceExtensions.emplace_back("VK_MVK_macos_surface");
#endif
    std::unique_ptr<VulkanRuntime> runtime(new VulkanRuntime(*surface.get(), requiredInstanceExtensions, requiredValidationLayers));
    // swap chain
    VulkanSwapChain* swapchain = nullptr;
    runtime->createSwapChain(swapchain, metalLayer, 1);
    
    // vertex and fragment shader module
    std::vector<uint8_t> vertexShader = readShaderBinary("triangle_vert.spv");
    std::vector<uint8_t> fragShader = readShaderBinary("triangle_frag.spv");
    
    std::unique_ptr<Program> pg(new Program);
    pg->withVertexShader(vertexShader.data(), vertexShader.size());
    pg->withFragmentShader(fragShader.data(), fragShader.size());
    VulkanProgram* program = nullptr;
    std::string programName = "triangle";
    runtime->createProgram(program, *pg, programName);
    
    VulkanTexture *colorTexture = nullptr;
    VulkanTexture *depthTexture = nullptr;
    VulkanTexture *stencilTexture = nullptr;
    
    // color texture
    uint32_t maxLevel = getTextureMipmapLevels((uint32_t)FRAME_WIDTH, (uint32_t)FRAME_HEIGHT);
    runtime->createTexture(colorTexture, SamplerType::SAMPLER_2D, 1, TextureFormat::RGBA8, 1, FRAME_WIDTH, FRAME_HEIGHT, 1, TextureUsage::COLOR_ATTACHMENT | TextureUsage::SAMPLEABLE);
    // depth and stencil texture
    runtime->createTexture(depthTexture, SamplerType::SAMPLER_2D, 1, TextureFormat::DEPTH32F, 1, FRAME_WIDTH, FRAME_HEIGHT, 1, TextureUsage::DEPTH_ATTACHMENT);

    VulkanAttachment color_attachs[MAX_SUPPORTED_RENDER_TARGET_COUNT];
    color_attachs[0].texture = colorTexture;
    VulkanAttachment color_attach = createAttachment(color_attachs[0]);
    color_attach.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // rendering on screen
    color_attachs[0] = color_attach;
    // set raster state
    RasterStateT rasterState = {};
    rasterState.colorWrite = true;
    rasterState.depthWrite = true;
    rasterState.depthFunc = SamplerCompareFunc::L;
    rasterState.culling = CullingMode::NONE;
    rasterState.alphaToCoverage = false;
    rasterState.inverseFrontFaces = true; // clockwise
    
    PolygonOffset polygonOffset = {};
    Viewport scissor = {0, 0, FRAME_WIDTH, FRAME_HEIGHT};
    
    PipelineState pipelineState;
    pipelineState.program.reset(program);
    pipelineState.rasterState = rasterState;
    pipelineState.polygonOffset = polygonOffset;
    pipelineState.scissor = scissor;
    // create primitive
    std::vector<Vec2> trianglePositions = {
        { 0.5, -0.5 },
        { 0.5, 0.5 },
        { -0.5, 0.5 }
    };

    std::vector<Vec3> triangleColors = {
        { 1.0, 0.0, 0.0 },
        { 0.0, 1.0, 0.0 },
        { 0.0, 0.0, 1.0 }
    };
    
    std::vector<uint32_t> triangle_indexes = { 0, 1, 2 };
    
    BufferDescriptor posDesc((void const*)trianglePositions.data(), trianglePositions.size() * sizeof(Vec2));
    BufferDescriptor colorDesc((void const*)triangleColors.data(), triangleColors.size() * sizeof(Vec3));
    BufferDescriptor indexDesc((void const*)triangle_indexes.data(), triangle_indexes.size() * sizeof(uint32_t));
    
    // pos buffer object
    VulkanBufferObject *positionBufferObject = nullptr;
    runtime->createBufferObject(positionBufferObject, uint32_t(trianglePositions.size() * sizeof(Vec2)));
    // color buffer object
    VulkanBufferObject *colorBufferObject = nullptr;
    runtime->createBufferObject(colorBufferObject, uint32_t(triangleColors.size() * sizeof(Vec3)));
    // vertex buffer
    VulkanVertexBuffer *vertexBuffer = nullptr;
    
    AttributeArray attributes = {};
    Attribute attr0;
    attr0.offset = 0;
    attr0.stride = sizeof(Vec2);
    attr0.buffer = 0;
    attr0.type = ElementType::FLOAT2;
    attr0.flags = 0;
    attributes[0] = attr0;
    
    Attribute attr1;
    attr1.offset = 0;
    attr1.stride = sizeof(Vec3);
    attr1.buffer = 1;
    attr1.type = ElementType::FLOAT3;
    attr1.flags = 0;
    attributes[1] = attr1;
    
    runtime->createVertexBuffer(vertexBuffer, 2, 2, 3, attributes);
    runtime->setVertexBufferObject(vertexBuffer, 0, positionBufferObject);
    runtime->setVertexBufferObject(vertexBuffer, 1, colorBufferObject);
    
    // index buffer
    VulkanIndexBuffer *indexBuffer = nullptr;
    runtime->createIndexBuffer(indexBuffer, ElementType::UINT, 3);
    runtime->updateIndexBuffer(indexBuffer, indexDesc, 0);
    
    VulkanRenderPrimitive *renderPrimitive = nullptr;
    runtime->createRenderPrimitive(renderPrimitive);
    runtime->setRenderPrimitiveBuffer(renderPrimitive, vertexBuffer, indexBuffer);
    runtime->setRenderPrimitiveRange(renderPrimitive, PrimitiveType::TRIANGLES, 0, 0, 2, 3);
    
    VulkanRenderTarget *renderTarget = nullptr;
    runtime->createRenderTarget(renderTarget, FRAME_WIDTH, FRAME_HEIGHT, 1, color_attachs, *depthTexture, *stencilTexture);
    // render pass param
    RenderPassParams renderPassParam; 
    renderPassParam.flags.clear = TargetBufferFlags::COLOR | TargetBufferFlags::DEPTH;
    renderPassParam.flags.discardStart = TargetBufferFlags::ALL;
    renderPassParam.flags.discardEnd = TargetBufferFlags::NONE;
    renderPassParam.viewport = {0, 0, FRAME_WIDTH, FRAME_HEIGHT};
    renderPassParam.clearColor = {0.0, 0.0, 0.0, 1.0};
    // should be within the [0.0, 1.0] range, if VK_EXT_depth_range_unrestricted extension is not enabled
    renderPassParam.depthRange = {0.0, 1.0};
    
    // offscreen render
    // PixelBufferDescriptor pixelBufferObject;
    
    static int64_t frameCount = 0;
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        
        static float currentTime = 0.0;
        runtime->beginFrame(swapchain, (int64_t)currentTime, frameCount);
        // update vertex buffer and index buffer
        runtime->updateBufferObject(positionBufferObject, posDesc, 0);
        runtime->updateBufferObject(colorBufferObject, colorDesc, 0);
        runtime->updateIndexBuffer(indexBuffer, indexDesc, 0);
        // begin render pass
        runtime->beginRenderPass(renderTarget, renderPassParam);
        runtime->draw(pipelineState, renderPrimitive);
        runtime->endRenderPass();
        // end render pass
        // runtime->readPixels(color_texture, 0, 0, FRAME_WIDTH, FRAME_HEIGHT, pixelBufferObject);
        runtime->commit(swapchain);
        runtime->endFrame(frameCount);
        runtime->finish();
        // tick
        // runtime->update(currentTime);
        frameCount++;
    }
        
    // destroy all resoures
    runtime->destroyBufferObject(positionBufferObject);
    runtime->destroyBufferObject(colorBufferObject);
    runtime->destroyVertexBuffer(vertexBuffer);
    runtime->destroyIndexBuffer(indexBuffer);
    runtime->destroyRenderPrimitive(renderPrimitive);
    
    runtime->destroyTexture(colorTexture);
    runtime->destroyTexture(depthTexture);
    runtime->destroyRenderTarget(renderTarget);
    runtime->destroySwapChain(swapchain);
    
    runtime->terminate();
    
    glfwSetWindowShouldClose(window, GLFW_TRUE);
    glfwTerminate();

    return 0;
}
