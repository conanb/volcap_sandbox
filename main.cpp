#include <GL/glew.h>

#include <GLFW/glfw3.h>

#include <imgui.h>
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <iostream>

#include <librealsense2/rs.hpp>
#include <librealsense2/hpp/rs_processing.hpp>

#include <opencv2/calib3d.hpp>
#include <opencv2/aruco.hpp>
#include <opencv2/aruco/charuco.hpp>
#include <opencv2/imgcodecs.hpp>

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

void copyFrameToGLTexture(GLuint& gl_handle, const rs2::video_frame& frame);

struct rs2_gl_textures {
    GLuint color;
    GLuint depth;
};

static inline void createBoard()
{
    cv::Ptr<cv::aruco::Dictionary> dictionary = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_6X6_250);
    cv::Ptr<cv::aruco::CharucoBoard> board = cv::aruco::CharucoBoard::create(5, 7, 0.04f, 0.02f, dictionary);
    cv::Mat boardImage;
    board->draw(cv::Size(600, 500), boardImage, 10, 1);
    cv::imwrite("BoardImage.jpg", boardImage);
}

int main() {

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    GLFWwindow* window = glfwCreateWindow(1920, 1080, "RealSense Test", nullptr, nullptr);
    glfwMakeContextCurrent(window);

    glfwSwapInterval(0);

    glewInit();

    glClearColor(0,0,0,1);

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init();

//    rs2::pointcloud pc;
 //   rs2::points points; 

  //  createBoard();
    
    rs2::context rsContext;

    std::vector<rs2_gl_textures> rs_textures;
    std::vector<rs2::pipeline> pipelines;
    for (auto&& dev : rsContext.query_devices())
    {
        rs2::pipeline pipe(rsContext);
        rs2::config cfg;
        cfg.enable_device(dev.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER));
        pipe.start(cfg);
        pipelines.emplace_back(pipe);

        rs_textures.push_back(rs2_gl_textures{ 0, 0 });
    }

    rs2::colorizer colorizer;

    while (!glfwWindowShouldClose(window)) {

        glfwPollEvents(); 

        int index = 0;
        std::vector<rs2::frame> new_frames;
        for (auto&& pipe : pipelines) {
            rs2::frameset frames;
            if (pipe.poll_for_frames(&frames)) {

                auto depth = frames.get_depth_frame();
                auto color_depth = colorizer.colorize(depth);
                copyFrameToGLTexture(rs_textures[index].depth, color_depth);

                auto color = frames.get_color_frame();
                copyFrameToGLTexture(rs_textures[index].color, color);

                // points = pc.calculate(depth);
                // pc.map_to(color);
            }
            index++;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        if (ImGui::BeginMainMenuBar()) {
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::EndMainMenuBar();
        }

        if (ImGui::Begin("RealSense")) {
            if (ImGui::BeginTable("Frames", 2)) {
                for (auto&& textures : rs_textures) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    if (textures.color)
                        ImGui::Image((void*)(intptr_t)textures.color, ImVec2(640, 480));
                    ImGui::TableNextColumn();
                    if (textures.depth)
                        ImGui::Image((void*)(intptr_t)textures.depth, ImVec2(640, 480));
                }
                ImGui::EndTable();
            }
            ImGui::End();
        }

        ImGui::Render();

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

void copyFrameToGLTexture(GLuint& gl_handle, const rs2::video_frame& frame)
{
    if (!frame) return;

    if (!gl_handle)
        glGenTextures(1, &gl_handle);
    GLenum err = glGetError();

    auto format = frame.get_profile().format();
    auto width = frame.get_width();
    auto height = frame.get_height();

    glBindTexture(GL_TEXTURE_2D, gl_handle);

    switch (format)
    {
    case RS2_FORMAT_RGB8:
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, frame.get_data());
        break;
    case RS2_FORMAT_RGBA8:
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, frame.get_data());
        break;
    case RS2_FORMAT_Y8:
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, frame.get_data());
        break;
    case RS2_FORMAT_Y10BPACK:
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width, height, 0, GL_LUMINANCE, GL_UNSIGNED_SHORT, frame.get_data());
        break;
    default:
        throw std::runtime_error("The requested format is not supported by this demo!");
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}