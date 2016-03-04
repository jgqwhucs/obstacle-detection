//
// Created by paul on 09/05/15 (dd/mm/yy, of course)
//

#include <geometry/HorizonFactory.h>
#include <camera/lepton/ThermalCameraStream.h>
#include "RigRunner.h"

#define APP_NAME "rig_record"

bool stop_record = false;

static void hangup_handler(int signum) {
    if (signum == SIGHUP) {
        stop_record = true;
    }
}

void setup_sighandler() {
    struct sigaction sa;

    sa.sa_handler = hangup_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART; /* Restart functions if interrupted by handler */
    if (sigaction(SIGHUP, &sa, NULL) == -1)
        cerr << "Failed to initialize signal handler for " << APP_NAME << endl;
}

void record(char *output_dir, bool verbose) {

    Lepton lepton(0, 1);
    ThermalCameraStream leptonStream(lepton);
    ParallelIMU imu;
    SimpleRescaler rescaler;
    HorizonFactory horizonFactory(LeptonCameraSpecifications);

    Image16bit frame(LeptonCameraSpecifications.pixelHeight, LeptonCameraSpecifications.pixelWidth);
    Image8bit displayed(60, 80);
    int frame_counter = 1;
    char img_name[128];
    char imu_file_name[128];

    std::ofstream imuLog;

    // timing
    std::chrono::time_point <std::chrono::system_clock> start, end;
    float leptonPeriodSeconds = 1 / Lepton::FPS;

    sprintf(imu_file_name, "%s/imuLog.txt", output_dir);
    imuLog.open(imu_file_name);

    cout << "Connecting to screen" << endl;

    Display *display = DisplayUtils::connectToDisplay();


    cout << "Starting Capture" << endl;

    while (!stop_record) {

        // Perform FFC every FFC_FREQ_SEC seconds
/*        if ( (frame_counter / LEPTON_FPS) % FFC_FREQ_SEC == FFC_FREQ_SEC - 1) {
            cout << "Performing FFC" << endl;
            lepton.performFFC();
            usleep(600 * 1000);
        }
*/

        frame = leptonStream.nextImage();

        // save the current frame as a .png file
        sprintf(img_name, "%s/raw/img_%06d.png", output_dir, frame_counter);
        imwrite(img_name, frame);
        imuLog << imu.getOrientation().toDataString();

        frame_counter++;

        // convert to 8 bit and display
        rescaler.scale16bitTo8bit(frame, displayed);
        Horizon horizon = horizonFactory.makeHorizon(imu.getOrientation());
        DisplayUtils::displayFrameWithHorizonLine(displayed, horizon, *display);
    }

    imuLog.flush();
    imuLog.close();
    cout << "Recording received stopping signal " << "and terminated gracefully." << endl;

}

void printUsage(int argc, char **argv) {
    cout << "Usage: rig_record <output_dir>" << endl;
    cout << "You entered: " << endl;
    for (int i = 0; i < argc; i++)
        cout << argv[i];
    cout << endl;
}

int main(int argc, char **argv) {

    if (argc < 2) {
        printUsage(argc, argv);
        return 1;
    }

    char *output_dir = argv[1];
    setup_sighandler();

    if (argc == 2) {
        record(output_dir);
    }

    else if (argc == 3) {
        char *arg2 = argv[2];
        if (!strcmp(arg2, "--silent"))
            record(output_dir, false);
    }

    else
        printUsage(argc, argv);

    return 0;
}
