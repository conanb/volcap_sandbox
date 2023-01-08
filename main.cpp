#include <GL/glew.h>

#include <GLFW/glfw3.h>

#include <imgui.h>
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <iostream>

#include <librealsense2/rs.hpp>
#include <librealsense2/hpp/rs_processing.hpp>

#include <opencv2/opencv.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/aruco.hpp>
#include <opencv2/aruco/charuco.hpp>
#include <opencv2/imgcodecs.hpp>

#include "Visualiser.h"

#include  <Eigen/Geometry>

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

void copyFrameToGLTexture(GLuint& gl_handle, const rs2::video_frame& frame);

static cv::Mat frame_to_mat(const rs2::frame& f);
static cv::Mat depth_frame_to_meters(const rs2::depth_frame& f);

struct rs2_gl_textures {
    GLuint color = 0;
    GLuint depth = 0;
    GLuint grabCut = 0;

    bool enabled = false;
    bool rgbOn = false;
    bool depthOn = false;

    float depthMin = 0;
    float depthMax = 10;

    float cameraY = 0;

    bool locked = false;

    Eigen::Affine3f transform = Eigen::Affine3f::Identity();

    rs2::pointcloud pc;
    rs2::points points; 
};

static Eigen::Matrix4f createPerspectiveMatrix(float yFoV, float aspect, float near, float far)
{
    Eigen::Matrix4f out = Eigen::Matrix4f::Zero();

    const float
        y_scale = (float)1.0f / tan((yFoV / 2.0f) * (std::numbers::pi_v<float> / 180.0f)),
        x_scale = y_scale / aspect,
        frustum_length = far - near;

    out(0, 0) = x_scale;
    out(1, 1) = y_scale;
    out(2, 2) = -((far + near) / frustum_length);
    out(3, 2) = -1.0;
    out(2, 3) = -((2 * near * far) / frustum_length);

    return out;
}

static inline void createBoard()
{
    cv::Ptr<cv::aruco::Dictionary> dictionary = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_6X6_250);
    cv::Ptr<cv::aruco::CharucoBoard> board = cv::aruco::CharucoBoard::create(5, 7, 0.04f, 0.02f, dictionary);
    cv::Mat boardImage;
    board->draw(cv::Size(1200, 1000), boardImage);// , 1, 1);
    cv::imwrite("BoardImage.jpg", boardImage);
}

int main() {

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    GLFWwindow* window = glfwCreateWindow(1920, 1080, "RealSense Test", nullptr, nullptr);
    glfwMakeContextCurrent(window);

    glfwSwapInterval(0);

    glewInit();

    auto gizmos = Visualiser::create();

    glClearColor(0.1f,0.1f,0.1f,1);

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init();

//    createBoard();
    
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

        rs_textures.push_back(rs2_gl_textures());
    }

    rs2::colorizer colorizer;
    colorizer.set_option(RS2_OPTION_COLOR_SCHEME, 2);
    rs2::align align_to(RS2_STREAM_COLOR);

    // We are using StructuringElement for erode / dilate operations
    auto gen_element = [](int erosion_size)
    {
        return cv::getStructuringElement(cv::MORPH_RECT,
            cv::Size(erosion_size + 1, erosion_size + 1),
            cv::Point(erosion_size, erosion_size));
    };

    const int erosion_size = 3;
    auto erode_less = gen_element(erosion_size);
    auto erode_more = gen_element(erosion_size * 2);

    // The following operation is taking grayscale image,
    // performs threashold on it, closes small holes and erodes the white area
    auto create_mask_from_depth = [&](cv::Mat& depth, int thresh, cv::ThresholdTypes type)
    {
        cv::threshold(depth, depth, thresh, 255, type);
        dilate(depth, depth, erode_less);
        erode(depth, depth, erode_more);
    };

    // Skips some frames to allow for auto-exposure stabilization
    for (int i = 0; i < 10; i++) pipelines[0].wait_for_frames();

    auto proj = createPerspectiveMatrix(45, 1920/1080.f, 0.01f, 5.0f);

    auto position = Eigen::Vector3f{ 1.5f, 1.5f, 1.5f };
    auto up = Eigen::Vector3f(0, 1, 0);

    Eigen::Matrix3f camAxes;
    camAxes.col(2) = (position).normalized();
    camAxes.col(0) = up.cross(camAxes.col(2)).normalized();
    camAxes.col(1) = camAxes.col(2).cross(camAxes.col(0)).normalized();
    auto orientation = Eigen::Quaternionf(camAxes);
    auto viewA = Eigen::Affine3f();
    viewA.linear() = orientation.conjugate().toRotationMatrix();
    viewA.translation() = -(viewA.linear() * position);

    auto view = viewA.matrix();

    while (!glfwWindowShouldClose(window)) {

        glfwPollEvents(); 

        // clear andd add 2m grid
        gizmos->clear(); {
            // grid 2mX2m 
            gizmos->addLine({ 1, 0, 0, 1 }, { -1, 0, 0, 1 }, { 0, 0, 0, 1 });
            gizmos->addLine({ 0, 0, 1, 1 }, { 0, 0, -1, 1 }, { 0, 0, 0, 1 });
            gizmos->addLine({ 1, 0, 1, 1 }, { -1, 0, 1, 1 }, { 0, 0, 0, 1 });
            gizmos->addLine({ 1, 0, 1, 1 }, { 1, 0, -1, 1 }, { 0, 0, 0, 1 });
            gizmos->addLine({ -1, 0, -1, 1 }, { 1, 0, -1, 1 }, { 0, 0, 0, 1 });
            gizmos->addLine({ -1, 0, -1, 1 }, { -1, 0, 1, 1 }, { 0, 0, 0, 1 });

            for (int i = 0; i < 19; ++i) {
                if (i == 9) continue;
                gizmos->addLine({ -.9f + i * 0.1f, 0, 1, 1 }, { -.9f + i * 0.1f, 0, -1, 1 }, { 0.5f, 0.5f, 0.5f, 1 });
                gizmos->addLine({ 1, 0, -.9f + i * 0.1f, 1 }, { -1, 0, -.9f + i * 0.1f, 1 }, { 0.5f, 0.5f, 0.5f, 1 });
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        if (ImGui::BeginMainMenuBar()) {
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::EndMainMenuBar();
        }

        int index = 0;
        std::vector<rs2::frame> new_frames;
        for (auto&& pipe : pipelines) {
            rs2::frameset frames;
            if ((rs_textures[index].rgbOn || rs_textures[index].depthOn) /*&& 
                pipe.poll_for_frames(&frames)*/) {

                frames = pipe.wait_for_frames();

         //       auto aligned_set = align_to.process(frames);
         //       auto depth_processed = aligned_set.get_depth_frame();

                if (rs_textures[index].depthOn) {
                    auto depth = frames.get_depth_frame();
                    auto color_depth = colorizer.colorize(depth);// _processed);
                    copyFrameToGLTexture(rs_textures[index].depth, color_depth);

                    // rs_textures[index].points = rs_textures[index].pc.calculate(rs_textures[index].depth);
                }

                if (rs_textures[index].rgbOn) {
                    auto color = frames.get_color_frame();
                    copyFrameToGLTexture(rs_textures[index].color, color);
                }

                // rs_textures[index].points = rs_textures[index].pc.calculate(rs_textures[index].depth);
                // pc.map_to(color);
            }

            rs2::device rs_dev = pipe.get_active_profile().get_device();
            ImGui::SetNextWindowSize(ImVec2{0,0});
            rs_textures[index].enabled = ImGui::Begin(rs_dev.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER));
            if (rs_textures[index].enabled) {
                ImGui::Checkbox(" - RBB", &rs_textures[index].rgbOn);
                ImGui::Checkbox(" - D", &rs_textures[index].depthOn); 

                ImGui::SliderFloat(" - Min", &rs_textures[index].depthMin, 0, 10);
                ImGui::SliderFloat(" - Max", &rs_textures[index].depthMax, 0, 10);
                ImGui::Checkbox(" - Locked", &rs_textures[index].locked);
                ImGui::InputFloat(" - Y Offset", &rs_textures[index].cameraY);

                if (rs_textures[index].rgbOn)
                    ImGui::Image((void*)(intptr_t)rs_textures[index].color, ImVec2(320, 240));
                if (rs_textures[index].depthOn)
                    ImGui::Image((void*)(intptr_t)rs_textures[index].depth, ImVec2(320, 240));

                gizmos->addTransform(rs_textures[index].transform.matrix(), 0.1f);
            }
                ImGui::End();
            index++;
        }


        ImGui::Render();

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        gizmos->draw(view, proj);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    gizmos->destroy();

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

// Convert rs2::frame to cv::Mat
static cv::Mat frame_to_mat(const rs2::frame& f)
{
    using namespace cv;
    using namespace rs2;

    auto vf = f.as<video_frame>();
    const int w = vf.get_width();
    const int h = vf.get_height();

    if (f.get_profile().format() == RS2_FORMAT_BGR8)
    {
        return Mat(Size(w, h), CV_8UC3, (void*)f.get_data(), Mat::AUTO_STEP);
    }
    else if (f.get_profile().format() == RS2_FORMAT_RGB8)
    {
        auto r_rgb = Mat(Size(w, h), CV_8UC3, (void*)f.get_data(), Mat::AUTO_STEP);
        Mat r_bgr;
        cvtColor(r_rgb, r_bgr, COLOR_RGB2BGR);
        return r_bgr;
    }
    else if (f.get_profile().format() == RS2_FORMAT_Z16)
    {
        return Mat(Size(w, h), CV_16UC1, (void*)f.get_data(), Mat::AUTO_STEP);
    }
    else if (f.get_profile().format() == RS2_FORMAT_Y8)
    {
        return Mat(Size(w, h), CV_8UC1, (void*)f.get_data(), Mat::AUTO_STEP);
    }
    else if (f.get_profile().format() == RS2_FORMAT_DISPARITY32)
    {
        return Mat(Size(w, h), CV_32FC1, (void*)f.get_data(), Mat::AUTO_STEP);
    }

    throw std::runtime_error("Frame format is not supported yet!");
}

// Converts depth frame to a matrix of doubles with distances in meters
static cv::Mat depth_frame_to_meters(const rs2::depth_frame& f)
{
    cv::Mat dm = frame_to_mat(f);
    dm.convertTo(dm, CV_64F);
    dm = dm * f.get_units();
    return dm;
}
