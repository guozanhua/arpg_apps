#include <HAL/Camera/CameraDevice.h>
#include <HAL/IMU/IMUDevice.h>
#include <HAL/Posys/PosysDevice.h>
#include <HAL/Encoder/EncoderDevice.h>
#include <HAL/LIDAR/LIDARDevice.h>

#include <HAL/Utils/GetPot>
#include <HAL/Utils/TicToc.h>

#include <HAL/Messages/ImageArray.h>
#include <HAL/Messages/Logger.h>
#include <HAL/Messages/Matrix.h>

using std::placeholders::_1;

class SensorLogger {
public:
  SensorLogger() : num_channels_(0), base_width_(0), base_height_(0),
    has_camera_(false), has_imu_(false), has_posys_(false),
    has_encoder_(false), has_lidar_(false),
    is_running_(true), frame_number_(0),
    logger_(hal::Logger::GetInstance())
  {
  }

  void start_paused( void ) {
    is_running_ = false;
  }

  virtual ~SensorLogger() {}

  void Run() {
    bool should_quit_ = false;
    bool logging_enabled_ = true;
    RegisterCallbacks();

    std::cout << "Press RETURN to stop recording and write to logfile." << std::endl;

    bool got_first_image = false;
    bool capture_success = false;
    std::shared_ptr<hal::ImageArray> last_images;
    for (; !should_quit_; ++frame_number_) {
      std::cin.ignore( std::numeric_limits<std::streamsize>::max(), '\n' );
      if (std::cin.get() == '\n') {
        std::cout << "Writing to logfile." << std::endl;
        should_quit_ = true;
      }

      const bool go = is_running_;

      std::shared_ptr<hal::ImageArray> images = hal::ImageArray::Create();

      if (go && num_channels_) {
        capture_success = camera_.Capture(*images);
        if (!got_first_image && capture_success) {
          got_first_image = true;
        }
      }

      // Just display the last images if we didn't get new ones.
      if (!go || !capture_success) {
        images = last_images;
      }

      if (logging_enabled_ && is_running_) {
        if (hal::Logger::GetInstance().IsLogging() == false) {
          logger_.LogToFile("", "sensors");
        }
        if (capture_success) {
          LogCamera(images.get());
        }
      }

      last_images = images;

    }
  }

  void set_camera(const std::string& cam_uri) {
    try {
      camera_ = hal::Camera(cam_uri);
    } catch (const hal::DeviceException& e) {
      std::cerr << "SensorLogger: Camera failed to open! Exception: "
                << e.what() << std::endl;
      abort();
    }
    has_camera_ = true;

    num_channels_ = camera_.NumChannels();
    base_width_ = camera_.Width();
    base_height_ = camera_.Height();

    for (size_t ii=0; ii < num_channels_; ++ii) {
      std::cout << "\t" << camera_.Width(ii) << "x"
                << camera_.Height(ii) << std::endl;
    }
  }

  void set_imu(const std::string& imu_uri) {
    imu_ = hal::IMU(imu_uri);
    has_imu_ = true;
  }

  void set_posys(const std::string& posys_uri) {
    posys_ = hal::Posys(posys_uri);
    has_posys_ = true;
  }

  void set_encoder(const std::string& encoder_uri)
  {
    encoder_ = hal::Encoder(encoder_uri);
    has_encoder_ = true;
  }

  void set_lidar(const std::string& lidar_uri)
  {
    lidar_ = hal::LIDAR(lidar_uri);
    has_lidar_ = true;
  }

protected:
  void RegisterCallbacks() {
    if (has_posys_) {
      posys_.RegisterPosysDataCallback(std::bind(&SensorLogger::Posys_Handler,
                                                 this, _1));
      std::cout << "- Registering Posys device." << std::endl;
    }

    if (has_imu_) {
      imu_.RegisterIMUDataCallback(
            std::bind(&SensorLogger::IMU_Handler, this, _1));
      std::cout << "- Registering IMU device." << std::endl;
    }

    if (has_encoder_){
      encoder_.RegisterEncoderDataCallback(
            std::bind(&SensorLogger::Encoder_Handler, this, _1));
      std::cout << "- Registering Encoder device." << std::endl;
    }

    if (has_lidar_){
      lidar_.RegisterLIDARDataCallback(
            std::bind(&SensorLogger::LIDAR_Handler, this, _1));
      std::cout << "- Registering LIDAR device." << std::endl;
    }
  }

  void LogCamera(hal::ImageArray* images) {
    if (!has_camera_) return;

    hal::Msg pbMsg;
    pbMsg.set_timestamp(hal::Tic());
    pbMsg.mutable_camera()->Swap(&images->Ref());
    logger_.LogMessage(pbMsg);
  }

  void IMU_Handler(hal::ImuMsg& IMUdata) {
    hal::Msg pbMsg;
    pbMsg.set_timestamp(hal::Tic());
    pbMsg.mutable_imu()->Swap(&IMUdata);
    logger_.LogMessage(pbMsg);
  }

  void Posys_Handler(hal::PoseMsg& PoseData) {
    std::cout << "Posys Id: " << PoseData.id() << ". Data: ";
    for (int ii = 0; ii < PoseData.pose().data_size(); ++ii) {
      std::cout << PoseData.pose().data(ii) << " ";
    }
    std::cout << std::endl;

    hal::Msg pbMsg;
    pbMsg.set_timestamp(hal::Tic());
    pbMsg.mutable_pose()->Swap(&PoseData);
    logger_.LogMessage(pbMsg);
  }

  void Encoder_Handler(hal::EncoderMsg& EncoderData) {
    std::cout << "Encoder: ";
    for (int ii = 0; ii < EncoderData.label_size(); ++ii) {
      std::cout << EncoderData.label(ii) << ": " << EncoderData.data(ii) <<
                   ", ";
    }
    std::cout << std::endl;

    hal::Msg pbMsg;
    pbMsg.set_timestamp(hal::Tic());
    pbMsg.mutable_encoder()->Swap(&EncoderData);
    logger_.LogMessage(pbMsg);
  }

  void LIDAR_Handler(hal::LidarMsg& LidarData)
  {
    //std::cout << "Got LIDAR data..." << std::endl;
    hal::Msg pbMsg;
    pbMsg.set_timestamp(hal::Tic());
    pbMsg.mutable_lidar()->Swap(&LidarData);
    logger_.LogMessage(pbMsg);
  }

private:
  size_t num_channels_, base_width_, base_height_;
  bool has_camera_, has_imu_, has_posys_, has_encoder_, has_lidar_;
  bool is_running_;
  int frame_number_;
  hal::Camera camera_;
  hal::IMU imu_;
  hal::Encoder encoder_;
  hal::Posys posys_;
  hal::LIDAR lidar_;
  hal::Logger& logger_;
};

int main(int argc, char* argv[]) {
  GetPot cl_args(argc, argv);

  std::string cam_uri = cl_args.follow("", "-cam");
  std::string imu_uri = cl_args.follow("", "-imu");
  std::string posys_uri = cl_args.follow("", "-posys");
  std::string encoder_uri = cl_args.follow("","-encoder");
  std::string lidar_uri = cl_args.follow("","-lidar");
  bool start_paused_ = cl_args.search("-p");

  SensorLogger logger;
  if (start_paused_)
    logger.start_paused();
  if (!cam_uri.empty()) {
    logger.set_camera(cam_uri);
  }

  if (!imu_uri.empty()) {
    logger.set_imu(imu_uri);
  }

  if (!posys_uri.empty()) {
    logger.set_posys(posys_uri);
  }

  if (!encoder_uri.empty()) {
    logger.set_encoder(encoder_uri);
  }

  if (!lidar_uri.empty()) {
    logger.set_lidar(lidar_uri);
  }

  std::cout << "Press the ENTER key to START recording.";
  logger.Run();


}
