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
#include <opencv2/ccalib/multicalib.hpp>
#include <opencv2/aruco.hpp>
#include <opencv2/aruco/charuco.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/objdetect/objdetect.hpp>
#include <opencv2/highgui.hpp>

#include "Gizmos.h"
#include "Shader.h"

#include  <Eigen/Geometry>

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

void copyFrameToGLTexture(GLuint& gl_handle, const rs2::video_frame& frame);

static cv::Mat frame_to_mat(const rs2::frame& f);
static cv::Mat depth_frame_to_meters(const rs2::depth_frame& f);

class rs_camera {
public:

    rs_camera(rs2::pipeline& pipeline) : 
        pipe(pipeline), 
        id(pipeline.get_active_profile().get_device().get_info(RS2_CAMERA_INFO_SERIAL_NUMBER)) {
        loadCalibration();
    }
    ~rs_camera() {}

    rs2::pipeline pipe;
    std::string id;

    GLuint color = 0;
    GLuint depth = 0;
    GLuint grabCut = 0;

    bool rgbOn = false;
    bool depthOn = false;

    bool align = true;

    float depthMin = 0;
    float depthMax = 10;

    float cameraY = 0;

    bool locked = false;

    Eigen::Affine3f transform = Eigen::Affine3f::Identity();

    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint tbo = 0;

    rs2::pointcloud pc;
    rs2::points points; 
    rs2::align aligner = { RS2_STREAM_COLOR };

    rs2::frameset lastFrames;

    bool calibrated = false;
    std::vector<cv::Mat> capturedFrames;
    cv::Mat calibrationMatrix;
    cv::Mat calibrationDistanceCoeffs;

    bool detectMarker = false;
    bool markerboardFound = false;

    void calibrate(cv::Ptr<cv::aruco::CharucoBoard>& board) {
        if (capturedFrames.size() == 0) return;

        std::vector<std::vector<cv::Point2f>> allCharucoCorners;
        std::vector<std::vector<int>> allCharucoIds;

        cv::Size imgSize;

        // detech board from each image in captured frames
        cv::Ptr<cv::aruco::DetectorParameters> params = cv::makePtr<cv::aruco::DetectorParameters>();
        params->cornerRefinementMethod = cv::aruco::CORNER_REFINE_NONE;

        for (auto& image : capturedFrames) {
            imgSize = cv::Size(image.size[1], image.size[0]);

            std::vector<int> markerIds;
            std::vector<std::vector<cv::Point2f> > markerCorners;
            cv::aruco::detectMarkers(image, cv::makePtr<cv::aruco::Dictionary>(board->dictionary), markerCorners, markerIds, params);
            //or
            //cv::aruco::detectMarkers(image, dictionary, markerCorners, markerIds, params);
            // if at least one marker detected
            if (markerIds.size() > 0) {
                //    cv::aruco::drawDetectedMarkers(imageCopy, markerCorners, markerIds);
                std::vector<cv::Point2f> charucoCorners;
                std::vector<int> charucoIds;
                cv::aruco::interpolateCornersCharuco(markerCorners, markerIds, image, board, charucoCorners, charucoIds);
                // if at least one charuco corner detected
                if (charucoIds.size() > 0) {

                    allCharucoCorners.push_back(charucoCorners);
                    allCharucoIds.push_back(charucoIds);
                    //        cv::aruco::drawDetectedCornersCharuco(imageCopy, charucoCorners, charucoIds, cv::Scalar(255, 0, 0));
                }
            }
        }

        std::vector<cv::Mat> rvecs, tvecs;
        int calibrationFlags = 0;

        // calibrate the lens for distortion etc
        // NOTE: WE WANT REPERROR TO BE AS CLOSE TO 0 AS POSSIBLE!
        double repError = cv::aruco::calibrateCameraCharuco(allCharucoCorners, allCharucoIds,
            board,
            imgSize,
            calibrationMatrix, calibrationDistanceCoeffs, rvecs, tvecs,
            calibrationFlags);

        capturedFrames.clear();
        calibrated = true;

        std::cout << "Calibrated " << id << ":" << std::endl;
        std::cout << "Error: " << repError << std::endl;
        std::cout << "Matrix: " << calibrationMatrix << std::endl;
        std::cout << "Distance Coeffs: " << calibrationDistanceCoeffs << std::endl;

        saveCalibration();
    }

    void loadCalibration() {
        cv::FileStorage file(std::format("./calibration/{}.cal", id), cv::FileStorage::READ);
        if (file.isOpened()) {
            file["camera_matrix"] >> calibrationMatrix;
            file["distance_coeffs"] >> calibrationDistanceCoeffs;
            calibrated = true;
        }
    }

    void saveCalibration() {

        if (calibrated) {
            cv::FileStorage file(std::format("./calibration/{}.cal", id), cv::FileStorage::WRITE);
            file << "camera_matrix" << calibrationMatrix;
            file << "distance_coeffs" << calibrationDistanceCoeffs;
        }
    }

    bool findCharucoBoard(cv::Ptr<cv::aruco::CharucoBoard>& board) {

        markerboardFound = false;

        cv::Ptr<cv::aruco::DetectorParameters> params = cv::aruco::DetectorParameters::create();

        auto color = lastFrames.get_color_frame();
        auto image = frame_to_mat(color);

        std::vector<int> markerIds;
        std::vector<std::vector<cv::Point2f> > markerCorners;
        cv::aruco::detectMarkers(image, board->dictionary, markerCorners, markerIds, params);

        // if at least one marker detected
        if (markerIds.size() > 0) {
            std::vector<cv::Point2f> charucoCorners;
            std::vector<int> charucoIds;
            cv::aruco::interpolateCornersCharuco(markerCorners, markerIds, image, board, charucoCorners, charucoIds, calibrationMatrix, calibrationDistanceCoeffs);
            // if at least one charuco corner detected
            if (charucoIds.size() > 0) {
                cv::Vec3d rvec, tvec;
                bool valid = cv::aruco::estimatePoseCharucoBoard(charucoCorners, charucoIds, board, calibrationMatrix, calibrationDistanceCoeffs, rvec, tvec);
                // if charuco pose is valid
                if (valid) {
                    //     cv::drawFrameAxes(imageCopy, cameraMatrix, distCoeffs, rvec, tvec, 0.1f);
                    markerboardFound = true;

                    // with the rvec and tvec we can create a matrix!
                }
            }
        }
    }

    void updateBuffers() {

        if (points.size() == 0) return;

        // update positions
        if (vbo == 0) {
            glGenBuffers(1, &vbo);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, points.size() * sizeof(rs2::vertex), points.get_vertices(), GL_DYNAMIC_DRAW);
        }
        else {
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferSubData(GL_ARRAY_BUFFER, 0, points.size() * sizeof(rs2::vertex), points.get_vertices());
        }

        // update textures
        if (tbo == 0) {
            glGenBuffers(1, &tbo);
            glBindBuffer(GL_ARRAY_BUFFER, tbo);
            glBufferData(GL_ARRAY_BUFFER, points.size() * sizeof(rs2::texture_coordinate), points.get_texture_coordinates(), GL_DYNAMIC_DRAW);
        }
        else {
            glBindBuffer(GL_ARRAY_BUFFER, tbo);
            glBufferSubData(GL_ARRAY_BUFFER, 0, points.size() * sizeof(rs2::texture_coordinate), points.get_texture_coordinates());
        }

        if (vao == 0) {
            glGenVertexArrays(1, &vao);
            glBindVertexArray(vao);

            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

            glBindBuffer(GL_ARRAY_BUFFER, tbo);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);

            glBindVertexArray(0);
        }
    }

    void draw(Shader::UniformBase* a_modelUniform, const Eigen::Affine3f& captureSpaceMatrix,
              Shader::UniformBase* a_cutoffMinUniform, Shader::UniformBase* a_cutoffMaxUniform) {

        if (points.size() == 0) return;

        a_cutoffMinUniform->bind(depthMin);
        a_cutoffMaxUniform->bind(depthMax);
        a_modelUniform->bind((captureSpaceMatrix * transform).matrix());

        glBindTexture(GL_TEXTURE_2D, color);

        glBindVertexArray(vao);
        glDrawArrays(GL_POINTS, 0, points.size());
    }
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

auto CalculateViewMatrixLookAt(const auto& from, const auto& to) {

    auto direction = from - to;

    Eigen::Matrix3f camAxes;
    camAxes.col(2) = (direction).normalized();
    camAxes.col(0) = Eigen::Vector3f::UnitY().cross(camAxes.col(2)).normalized();
    camAxes.col(1) = camAxes.col(2).cross(camAxes.col(0)).normalized();
    auto orientation = Eigen::Quaternionf(camAxes);
    auto viewA = Eigen::Affine3f();
    viewA.linear() = orientation.conjugate().toRotationMatrix();
    viewA.translation() = -(viewA.linear() * from);

    return viewA.matrix();
}

void updateCamera(GLFWwindow* window, Eigen::Vector3f& eyePosition, Eigen::Vector3f& eyeTarget) {

    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
        auto a = Eigen::AngleAxisf(std::numbers::pi_v<float> *-ImGui::GetIO().DeltaTime, Eigen::Vector3f::UnitY());

        auto offset = eyePosition - eyeTarget;
        eyePosition = eyeTarget + a * offset;
    }
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
        auto a = Eigen::AngleAxisf(std::numbers::pi_v<float> *ImGui::GetIO().DeltaTime, Eigen::Vector3f::UnitY());
        auto offset = eyePosition - eyeTarget;
        eyePosition = eyeTarget + a * offset;
    }

    auto dir = (eyeTarget - eyePosition).normalized();
    auto right = dir.cross(Eigen::Vector3f::UnitY()).normalized();

    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
        auto a = Eigen::AngleAxisf(std::numbers::pi_v<float> *ImGui::GetIO().DeltaTime, right);
        auto offset = eyePosition - eyeTarget;
        eyePosition = eyeTarget + a * offset;
    }
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
        auto a = Eigen::AngleAxisf(std::numbers::pi_v<float> *-ImGui::GetIO().DeltaTime, right);
        auto offset = eyePosition - eyeTarget;
        eyePosition = eyeTarget + a * offset;
    }

    if (glfwGetKey(window, GLFW_KEY_DELETE) == GLFW_PRESS) {
        eyePosition += right * -ImGui::GetIO().DeltaTime;
        eyeTarget += right * -ImGui::GetIO().DeltaTime;
    }
    if (glfwGetKey(window, GLFW_KEY_PAGE_DOWN) == GLFW_PRESS) {
        eyePosition += right * ImGui::GetIO().DeltaTime;
        eyeTarget += right * ImGui::GetIO().DeltaTime;
    }

    if (glfwGetKey(window, GLFW_KEY_HOME) == GLFW_PRESS) {
        eyePosition += dir * ImGui::GetIO().DeltaTime;
        eyeTarget += dir * ImGui::GetIO().DeltaTime;
    }
    if (glfwGetKey(window, GLFW_KEY_END) == GLFW_PRESS) {
        eyePosition += dir * -ImGui::GetIO().DeltaTime;
        eyeTarget += dir * -ImGui::GetIO().DeltaTime;
    }

    if (glfwGetKey(window, GLFW_KEY_PAGE_UP) == GLFW_PRESS) {
        eyePosition += Eigen::Vector3f::UnitY() * ImGui::GetIO().DeltaTime;
        eyeTarget += Eigen::Vector3f::UnitY() * ImGui::GetIO().DeltaTime;
    }
    if (glfwGetKey(window, GLFW_KEY_INSERT) == GLFW_PRESS) {
        eyePosition += Eigen::Vector3f::UnitY() * -ImGui::GetIO().DeltaTime;
        eyeTarget += Eigen::Vector3f::UnitY() * -ImGui::GetIO().DeltaTime;
    }
}

int main() {

    // WINDOW & GL SETUP
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    GLFWwindow* window = glfwCreateWindow(1920, 1080, "RealSense Test", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);
    glewInit();
    glClearColor(0.1f,0.1f,0.1f,1);

    // 3D GIZMOS
    auto gizmos = Gizmos::create();

    // IMGUI SETUP
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init();

    // CAMERA
    Eigen::Vector3f eyePosition{0,0.25,-1};
    Eigen::Vector3f eyeTarget = Eigen::Vector3f::Zero();
    Eigen::Matrix4f viewMatrix = CalculateViewMatrixLookAt(eyePosition, eyeTarget);
    Eigen::Matrix4f projectionMatrix = createPerspectiveMatrix(45, 1920 / 1080.f, 0.01f, 20.0f);

    // POINT CLOUD SHADER
    Shader* pcShader = new Shader("PC");
    pcShader->compileShaderFromFile(Shader::Stage::Vertex, "./shaders/pc.vert");
    pcShader->compileShaderFromFile(Shader::Stage::Geometry, "./shaders/pc.geom");
    pcShader->compileShaderFromFile(Shader::Stage::Fragment, "./shaders/pc.frag");
    pcShader->linkProgram();

    Shader::UniformBase* modelUniform = pcShader->getUniform("Model");
    Shader::UniformBase* viewUniform = pcShader->getUniform("View");
    Shader::UniformBase* projectionUniform = pcShader->getUniform("Projection");
    Shader::UniformBase* pointSizeUniform = pcShader->getUniform("PointSize");
    Shader::UniformBase* pointCloudColourUniform = pcShader->getUniform("PointCloudColour");
    Shader::UniformBase* cutoffMinUniform = pcShader->getUniform("CutoffMin");
    Shader::UniformBase* cutoffMaxUniform = pcShader->getUniform("CutoffMax");
    // texture uniform location (should already be 0, but oh well)
    pointCloudColourUniform->bind((unsigned int)0);

    float pointSize = 0.001f;

    Eigen::Affine3f captureSpaceMatrix = Eigen::Affine3f::Identity();

    // CALIBRATION
    auto arucoDictionary = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_5X5_50);
    auto charucoBoard = cv::aruco::CharucoBoard::create(5, 7, 0.04f, 0.02f, arucoDictionary);
 /*   cv::Mat boardImage, markerImage;
    charucoBoard->draw(cv::Size(1200, 1000), boardImage);
    cv::imwrite("DICT_5X5_50.jpg", boardImage);

    for (int i = 0; i < 50; ++i) {
        arucoDictionary.drawMarker(i, 400, markerImage, 1);
        std::string filename = std::format("./aruco/marker{}.png", i);
        cv::imwrite(filename, markerImage);
    }*/

    // COLLECT REALSENSE DEVICES
    rs2::context rsContext;
    std::vector<rs_camera> rs_devices;
    for (auto&& dev : rsContext.query_devices())
    {
        rs2::pipeline pipe(rsContext);
        rs2::config cfg;
        cfg.enable_device(dev.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER));
        cfg.enable_stream(RS2_STREAM_DEPTH);
        cfg.enable_stream(RS2_STREAM_COLOR, 1920, 1080, RS2_FORMAT_RGB8, 30);
        pipe.start(cfg);

        rs_devices.push_back(rs_camera(pipe));
    }

    if (rs_devices.size() == 0) {
        delete pcShader;
        gizmos->destroy();
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
        std::cout << "No RS Devices Detected!" << std::endl;
        return -2;
    }

    rs2::colorizer colorizer;
    colorizer.set_option(RS2_OPTION_COLOR_SCHEME, 2);

    // Skips some frames to allow for auto-exposure stabilization
    for (int i = 0; i < 10; i++) rs_devices[0].pipe.wait_for_frames();

    while (!glfwWindowShouldClose(window)) {

        glfwPollEvents(); 

        // clear andd add 2m grid
        gizmos->clear(); {
            // grid 2mX2m 
            gizmos->addLine({ 1, 0, 0 }, { -1, 0, 0 }, { 0, 0, 0, 1 });
            gizmos->addLine({ 0, 0, 1 }, { 0, 0, -1 }, { 0, 0, 0, 1 });
            gizmos->addLine({ 1, 0, 1 }, { -1, 0, 1 }, { 0, 0, 0, 1 });
            gizmos->addLine({ 1, 0, 1 }, { 1, 0, -1 }, { 0, 0, 0, 1 });
            gizmos->addLine({ -1, 0, -1 }, { 1, 0, -1 }, { 0, 0, 0, 1 });
            gizmos->addLine({ -1, 0, -1 }, { -1, 0, 1 }, { 0, 0, 0, 1 });

            for (int i = 0; i < 19; ++i) {
                if (i == 9) continue;
                gizmos->addLine({ -.9f + i * 0.1f, 0, 1 }, { -.9f + i * 0.1f, 0, -1 }, { 0.5f, 0.5f, 0.5f, 1 });
                gizmos->addLine({ 1, 0, -.9f + i * 0.1f }, { -1, 0, -.9f + i * 0.1f }, { 0.5f, 0.5f, 0.5f, 1 });
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        if (ImGui::BeginMainMenuBar()) {
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::SliderFloat("Point Size", &pointSize, 0, 1);
            ImGui::EndMainMenuBar();
        }

        for (auto& device : rs_devices) {

            ImGui::SetNextWindowSize(ImVec2{ 0,0 });

            if (ImGui::Begin(device.id.c_str())) {
                if (device.calibrated)
                    ImGui::Text("Calibrated");
                else
                    ImGui::Text("Not Calibrated!");

                ImGui::Checkbox(" - RBB", &device.rgbOn);
                ImGui::Checkbox(" - D", &device.depthOn);
                ImGui::Checkbox(" - Align", &device.align);
                ImGui::SliderFloat(" - Min", &device.depthMin, 0, 10);
                ImGui::SliderFloat(" - Max", &device.depthMax, 0, 10);
                ImGui::Checkbox(" - Locked", &device.locked);
                ImGui::InputFloat(" - Y Offset", &device.cameraY);

                auto pcSize = device.points.size();
                ImGui::LabelText(" - Points", "%d", pcSize);

                if ((device.rgbOn || device.depthOn) /*&&
                    pipe.poll_for_frames(&device.lastFrames)*/) {
                    device.lastFrames = device.pipe.wait_for_frames();

                    if (device.rgbOn) {
                        auto color = device.lastFrames.get_color_frame();
                        copyFrameToGLTexture(device.color, color);

                        device.pc.map_to(color);

                        if (ImGui::Button("Capture Frame")) {
                            device.capturedFrames.push_back(frame_to_mat(color));
                        }
                    }

                    if (device.depthOn) {

                        rs2::frameset aligned_set;
                        if (device.align)
                            aligned_set = device.aligner.process(device.lastFrames);

                        rs2::depth_frame depth = device.align ? aligned_set.get_depth_frame() : device.lastFrames.get_depth_frame();

                        auto color_depth = colorizer.colorize(depth);
                        copyFrameToGLTexture(device.depth, color_depth);

                        device.points = device.pc.calculate(depth);
                    }
                }

                if (device.capturedFrames.size() >= 10 &&
                    ImGui::Button("Calibrate")) {
                    device.calibrate(charucoBoard);
                }

                if (device.capturedFrames.size() > 0)
                ImGui::Text("Captured Frames: %d", device.capturedFrames.size());
                if (device.rgbOn) {
                    if (ImGui::Checkbox(" - Detect Board", &device.detectMarker)) {
                        device.findCharucoBoard(charucoBoard);

                        ImGui::Text("Found: %d", device.markerboardFound ? 1 : 0);
                    }
                }

                if (device.rgbOn)
                    ImGui::Image((void*)(intptr_t)device.color, ImVec2(320, 240));
                if (device.depthOn)
                    ImGui::Image((void*)(intptr_t)device.depth, ImVec2(320, 240));

                gizmos->addTransform(device.transform.matrix(), 0.1f);
            }
            ImGui::End();

            device.updateBuffers();
        }

        ImGui::Render();

        updateCamera(window, eyePosition, eyeTarget);
        viewMatrix = CalculateViewMatrixLookAt(eyePosition, eyeTarget);

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        gizmos->draw(viewMatrix, projectionMatrix);

        pcShader->bind();
        pointSizeUniform->bind(pointSize);
        viewUniform->bind(viewMatrix);
        projectionUniform->bind(projectionMatrix);
        for (auto& cam : rs_devices)
            if (cam.depthOn)
                cam.draw(modelUniform, captureSpaceMatrix, cutoffMinUniform, cutoffMaxUniform);
        pcShader->unBind();

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    delete pcShader;
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
