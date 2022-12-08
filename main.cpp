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

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

void copyFrameToGLTexture(GLuint& gl_handle, const rs2::video_frame& frame);

static cv::Mat frame_to_mat(const rs2::frame& f);
static cv::Mat depth_frame_to_meters(const rs2::depth_frame& f);

struct rs2_gl_textures {
    GLuint color;
    GLuint depth;
    GLuint grabCut;

    bool enabled;
    bool rgbOn;
    bool depthOn;
    bool maskOn;
};

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

    glClearColor(0,0,0,1);

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init();

//    rs2::pointcloud pc;
 //   rs2::points points; 

    createBoard();
    
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

        rs_textures.push_back(rs2_gl_textures{ 0, 0, 0, false, false, false, false });
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

    while (!glfwWindowShouldClose(window)) {

        glfwPollEvents(); 

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
            if (pipe.poll_for_frames(&frames)) {

                auto depth = frames.get_depth_frame();

                auto aligned_set = align_to.process(frames);
                auto depth_processed = aligned_set.get_depth_frame();

                auto color_depth = colorizer.colorize(depth_processed);
                copyFrameToGLTexture(rs_textures[index].depth, color_depth);

                auto color = frames.get_color_frame();
                copyFrameToGLTexture(rs_textures[index].color, color);

                // points = pc.calculate(depth);
                // pc.map_to(color);

                // this is so slow!
                // will be a lot faster when moved to compute
                if (rs_textures[index].enabled &&
                    rs_textures[index].maskOn) {
                    auto color_mat = frame_to_mat(aligned_set.get_color_frame());
                    auto bw_depth = depth_processed.apply_filter(colorizer);

                    // Generate "near" mask image:
                    auto near = frame_to_mat(bw_depth);
                    cvtColor(near, near, cv::COLOR_BGR2GRAY);
                    // Take just values within range [180-255]
                    // These will roughly correspond to near objects due to histogram equalization
                    create_mask_from_depth(near, 180, cv::THRESH_BINARY);

                    // Generate "far" mask image:
                    auto far = frame_to_mat(bw_depth);
                    cvtColor(far, far, cv::COLOR_BGR2GRAY);
                    far.setTo(255, far == 0); // Note: 0 value does not indicate pixel near the camera, and requires special attention 
                    create_mask_from_depth(far, 100, cv::THRESH_BINARY_INV);

                    // GrabCut algorithm needs a mask with every pixel marked as either:
                    // BGD, FGB, PR_BGD, PR_FGB
                    cv::Mat mask;
                    mask.create(near.size(), CV_8UC1);
                    mask.setTo(cv::Scalar::all(cv::GC_BGD)); // Set "background" as default guess
                    mask.setTo(cv::GC_PR_BGD, far == 0); // Relax this to "probably background" for pixels outside "far" region
                    mask.setTo(cv::GC_FGD, near == 255); // Set pixels within the "near" region to "foreground"

                    // Run Grab-Cut algorithm:
                    cv::Mat bgModel, fgModel;
                    grabCut(color_mat, mask, cv::Rect(), bgModel, fgModel, 1, cv::GC_INIT_WITH_MASK);

                    // Extract foreground pixels based on refined mask from the algorithm
                    cv::Mat3b foreground = cv::Mat3b::zeros(color_mat.rows, color_mat.cols);
                    color_mat.copyTo(foreground, (mask == cv::GC_FGD) | (mask == cv::GC_PR_FGD));

                    if (!rs_textures[index].grabCut)
                        glGenTextures(1, &rs_textures[index].grabCut);
                    if (foreground.data) {
                        glBindTexture(GL_TEXTURE_2D, rs_textures[index].grabCut);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, foreground.cols, foreground.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, foreground.data);
                        glBindTexture(GL_TEXTURE_2D, 0);
                    }
                }
            }

            rs2::device rs_dev = pipe.get_active_profile().get_device();
            ImGui::SetNextWindowSize(ImVec2{0,0});
            rs_textures[index].enabled = ImGui::Begin(rs_dev.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER));
            if (rs_textures[index].enabled) {
                ImGui::Checkbox(" - RBB", &rs_textures[index].rgbOn);
                ImGui::Checkbox(" - D", &rs_textures[index].depthOn); 
                ImGui::Checkbox(" - Mask", &rs_textures[index].maskOn);

                if (rs_textures[index].rgbOn)
                    ImGui::Image((void*)(intptr_t)rs_textures[index].color, ImVec2(320, 240));
                if (rs_textures[index].depthOn)
                    ImGui::Image((void*)(intptr_t)rs_textures[index].depth, ImVec2(320, 240));
                if (rs_textures[index].maskOn)
                    ImGui::Image((void*)(intptr_t)rs_textures[index].grabCut, ImVec2(320, 240));

            }
                ImGui::End();
            index++;
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