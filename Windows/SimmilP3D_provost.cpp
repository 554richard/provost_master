// Copyright (c) 2010-2019 Lockheed Martin Corporation. All rights reserved.
// Use of this file is bound by the PREPAR3D® SOFTWARE DEVELOPER KIT END USER LICENSE AGREEMENT 
//------------------------------------------------------------------------------
//
//  SimConnect Data Harvester Sample
//  
//  Description:
//              Requests data on the user object and outputs it to a CSV file.
//              If the sample application is ran from within the SimConnect
//              Samples solution, the CSV file will be created relative to the
//              SimmilPluginP project file. If the sample application is executed
//              directly, the CSV file will be created relative to the application.
//              Start and stop data harvesting from the Prepar3D Add-ons menu item.
//------------------------------------------------------------------------------

#define RHG_MAIN
#include "richard_UDP.h"

#include <Windows.h>
#include <tchar.h>
#include <thread>
#include <mutex>
#include <fstream>

#include "SimConnect.h"

#include "json.hpp"
using json = nlohmann::json;


#include "richard_logging.h"

int     g_bQuit = 0;
HANDLE  g_hSimConnect = NULL;
bool    g_verbose = true;

static const char* TITLE_STRING = "SimmilP3D";
static const char* START_STRING = "Start SimmilP3D Calibration";
static const char* STOP_STRING = "Stop SimmilP3D  Calibration";
static const char* RELOAD_CONFIG_STRING = "SimmilP3D  Reload .cfg";
static const char* RESET_MAIN_ARDUINO_STRING = "SimmilP3D Reset Main Arduino";

enum GROUP_ID
{
    GROUP0,
    INPUT0,
};

//Replaced enum with defines:
#define EVENT_ID_SIM_START 79
#define EVENT_ID_START_CALIBRATION 80
#define EVENT_ID_STOP_CALIBRATION  81
#define EVENT_ID_RELOAD_CONFIG  82
#define EVENT_ID_RESET_MAIN_MEGA 83
#define EVENT_ID_FLAPS_UP 84
#define EVENT_ID_FLAPS_DOWN 85
#define EVENT_ID_AXIS_LEFT_BRAKE_SET 86
#define EVENT_ID_AXIS_RIGHT_BRAKE_SET 87
#define EVENT_ID_GEAR_UP 88
#define EVENT_ID_GEAR_DOWN 89

#define DEFINITION_ID_USER_OBJECT 99

enum REQUEST_ID
{
    REQUEST_ID_USER_OBJECT_DATA,
};

#define CFG_FILENAME "SimmilP3D_provost.json"
#define MAX_CALIB_FILE 50
#define MSG_PER_CYCLE 200 // Max Number of USB messagea per loopback call.
#define MAX_ITEMS 2
#define MAX_OUTPUT_CONFIGS 200 //Max number of output entries in SimmilPluginX.cfg 
#define MAX_INPUT_CONFIGS 100  //Max number of input entries in SimmilPluginX.cfg 
#define CONFIG_ITEMS 12
#define MAX_SIMVAR_LEN 128 //Maximum length of a dataref
#define MAX_BOARDS 5
#define MAX_UDP_LEN 1024
#define MAX_USB_LEN 1024
#define A3967_FULL_CIRCLE 4096
#define SWITCH_WAIT_COUNT 1000   //If the plugin does get a switch this many times, it restarts the USB connection.
#define P3D_WAIT_TIME 25
#define P3D_ZEROING_WAIT_TIME 1000
#define IMPOSSIBLE_VALUE -999999.9

//Instrument types:
#define INST_UNKNOWN -1
#define INST_GAUGE 0
#define INST_SWITCH 1
#define INST_LIGHT 2

//Driver types:
#define DRIVER_UNKNOWN -1
#define DRIVER_NA 0
#define DRIVER_VID6606 1
#define DRIVER_A3967 2
#define DRIVER_A3967_360 3
#define DRIVER_THREEPHASE 4

//Switch types:
#define SW_TOG_DREF 3
#define SW_MOM_DREF 4
#define SW_TOG_CMD 5
#define SW_MOM_CMD 6
#define SW_ROT_CMD_UP 7
#define SW_ROT_CMD_DWN 8
#define SW_POT_AXIS 9

#define SWITCH_MSG_INCOMPLETE 1
#define SWITCH_MSG_COMPLETE 2
#define SWITCH_MSG_FAILED 3
#define SWITCH_MSG_GOT_BEACON 4

struct connect_status_counter {
    int noport = 0;
    int failed = 0;
    int nobeacon = 0;
    int created_handle = 0;
    int sent_request = 0;
    int request_received = 0;
    int VID6606_sent = 0;
    int VID6606_zeroed = 0;
    int A3967_sent = 0;
    int succeeded = 0;
};

double g_latest_sim_time;
int g_altitude_data_idx = 0;
double g_altitude_latest;
int g_chassis_idx = 0;
int g_gearleft_idx = 0;
int g_gearright_idx = 0;
double g_chassis_latest;
double g_gearleft_latest;
double g_gearright_latest;

//static struct UDPinfo g_UDPinfo;
static struct UDPconnection g_UDPconnect[2];
char g_directory_path[300];

//static FILE* g_fpLog;
static int g_p3dData_num = 0;
static bool g_calibrating = false;
bool g_simstarted = false;
bool g_A3967_zeroed = false;
bool g_VID6606_zeroed = false;
static struct connect_status_counter g_connect_status_counter;

json g_config;

/*
void increment_status_counter(void)
{
    if (g_USBinfo.status == CONNECT_NOPORT)
        g_connect_status_counter.noport++;
    else if (g_USBinfo.status == CONNECT_FAILED)
        g_connect_status_counter.failed++;
    else if (g_USBinfo.status == CONNECT_NOBEACON)
        g_connect_status_counter.nobeacon++;
    else if (g_USBinfo.status == CONNECT_CREATED_HANDLE)
        g_connect_status_counter.created_handle++;
    else if (g_USBinfo.status == CONNECT_SENT_REQUEST)
        g_connect_status_counter.sent_request++;
    else if (g_USBinfo.status == CONNECT_REQUEST_RECEIVED)
        g_connect_status_counter.request_received++;
    else if (g_USBinfo.status == CONNECT_VID6606_SENT)
        g_connect_status_counter.VID6606_sent++;
    else if (g_USBinfo.status == CONNECT_VID6606_ZEROED)
        g_connect_status_counter.VID6606_zeroed++;
    else if (g_USBinfo.status == CONNECT_A3967_SENT)
        g_connect_status_counter.A3967_sent++;
    else if (g_USBinfo.status == CONNECT_SUCCEEDED)
        g_connect_status_counter.succeeded++;
}

void report_status_counter(void)
{
    char debug_buffer[500];

    my_log(g_USBinfo.fpLog, "Status Report on Connection\n", 2);

    sprintf(debug_buffer, "CONNECT_NOPORT: %d\n", g_connect_status_counter.noport);
    my_log(g_USBinfo.fpLog, debug_buffer, 2);
    sprintf(debug_buffer, "CONNECT_FAILED: %d\n", g_connect_status_counter.failed);
    my_log(g_USBinfo.fpLog, debug_buffer, 2);
    sprintf(debug_buffer, "CONNECT_NOBEACON: %d\n", g_connect_status_counter.nobeacon);
    my_log(g_USBinfo.fpLog, debug_buffer, 2);
    sprintf(debug_buffer, "CONNECT_CREATED_HANDLE: %d\n", g_connect_status_counter.created_handle);
    my_log(g_USBinfo.fpLog, debug_buffer, 2);
    sprintf(debug_buffer, "CONNECT_SENT_REQUEST: %d\n", g_connect_status_counter.sent_request);
    my_log(g_USBinfo.fpLog, debug_buffer, 2);
    sprintf(debug_buffer, "CONNECT_REQUEST_RECEIVED: %d\n", g_connect_status_counter.request_received);
    my_log(g_USBinfo.fpLog, debug_buffer, 2);
    sprintf(debug_buffer, "CONNECT_VID6606_SENT: %d\n", g_connect_status_counter.VID6606_sent);
    my_log(g_USBinfo.fpLog, debug_buffer, 2);
    sprintf(debug_buffer, "CONNECT_VID6606_ZEROED: %d\n", g_connect_status_counter.VID6606_zeroed);
    my_log(g_USBinfo.fpLog, debug_buffer, 2);
    sprintf(debug_buffer, "CONNECT_A3967_SENT: %d\n", g_connect_status_counter.A3967_sent);
    my_log(g_USBinfo.fpLog, debug_buffer, 2);
}
*/

static int ReadConfigFile(void)
//Reads the Config info from SimmilPluginX.cfg into g_config.
{
    char debug_buffer[DEBUG_MSG_SIZE];

    //Open the file:
    char ff[300];
    strcpy_s(ff, sizeof(ff), g_directory_path);
    strcat_s(ff, sizeof(ff), CFG_FILENAME);

    std::ifstream fp(ff);

    if (!fp.is_open())
    {
        sprintf_s(debug_buffer, sizeof(debug_buffer), "Failed to open config file.\n");
        my_log(debug_buffer, g_log_dest);

        return -1;
    }

    sprintf_s(debug_buffer, sizeof(debug_buffer), "Reading SimmilP3D.json:\n");
    my_log(debug_buffer, g_log_dest);

    g_config = json::parse(fp);

    fp.close();
    return 1;
}

int DoCalibrationFromFile(json clb, double dval_in, double multiplier)
{
    int row, iclb_out;
    std::string clb_in_str, clb_out_str;
    double clb_in_double, clb_out_double;

    //Is the value outside the calibration file range?
    json first = *(clb.begin());
    clb_in_str = first["clb_in"];
    clb_in_double = std::stod(clb_in_str);

    if (dval_in < clb_in_double)
    {
        //Return the min calibration value: 
        clb_out_str = first["clb_out"];
        clb_out_double = std::stod(clb_out_str);
        iclb_out = int(clb_out_double * multiplier);
        return(iclb_out);
    }

    json last = clb.back();
    clb_in_str = last["clb_in"];
    clb_in_double = std::stod(clb_in_str);
    if (dval_in > clb_in_double)
    {
        //Return the max calibration value:
        clb_out_str = last["clb_out"];
        clb_out_double = std::stod(clb_out_str);
        iclb_out = int(clb_out_double * multiplier);
        return(iclb_out);
    }

    for (row = 0; row < clb.size(); row++)
    {
        clb_in_str = clb[row]["clb_in"];
        clb_in_double = std::stod(clb_in_str);
        if (dval_in > clb_in_double)
            continue;

        //Are we equal to the current row?
        if (dval_in == clb_in_double)
        {
            clb_out_str = clb[row]["clb_out"];
            clb_out_double = std::stod(clb_out_str);
            iclb_out = int(clb_out_double * multiplier);
            return(iclb_out);
        }

        //OK. We are smaller than the current row, and bigger than the previous. We are not smaller than the first row, 
        //that's been taken care of. So now we can do a straight interpolation:
        clb_in_str = clb[row - 1]["clb_in"];
        double below_in = std::stod(clb_in_str);
        clb_out_str = clb[row - 1]["clb_out"];
        double below_out = std::stod(clb_out_str);
        clb_in_str = clb[row]["clb_in"];
        double above_in = std::stod(clb_in_str);
        clb_out_str = clb[row]["clb_out"];
        double above_out = std::stod(clb_out_str);
        iclb_out = int(((dval_in - below_in) /
            (above_in - below_in)
            * (above_out - below_out)
            + below_out)
            * multiplier + 0.5);
        return(iclb_out);
    }

    //We should never get here:
    return 0;
}

double P3D_GetDoubleData(json row)
{
    double f10000, f1000;

    if (row["simvar"] == "SIMMIL_ALTITUDE_10000")
    {
        return(g_altitude_latest);
    }
    else if (row["simvar"] == "SIMMIL_ALTITUDE_1000")
    {
        f10000 = int(g_altitude_latest / 10000.0) * 10000.0;
        return (g_altitude_latest - f10000);
    }
    else if (row["simvar"] == "SIMMIL_ALTITUDE_100")
    {
        f1000 = int(g_altitude_latest / 1000.0) * 1000.0;
        return (g_altitude_latest - f1000);
    }

    if (row["p3d_idx"] < 0)
    {
        std::string calib_str = row["calib"];
        double calib_double = 0.0;
        if(calib_str != "")
            calib_double = std::stod(calib_str);
        return(calib_double);
    }

//    std::string p3d_latest_str = row["p3d_latest"];
//    double p3d_latest_double = std::stod(p3d_latest_str);
    return(row["p3d_latest"]);
}

double P3D_GetLightData(json row)
{
    if (row["simvar"] == "SIMMIL_GEAR_UP")
    {
        if (g_chassis_latest == 0)
            return(1);
        else
            return(0);
    }
    else if (row["simvar"] == "SIMMIL_GEAR_DOWN")
    {
        if (g_chassis_latest == 1)
            return(1);
        else
            return(0);
    }
    else if (row["simvar"] == "SIMMIL_GEAR_LEFT_DOWN")
    {
        if (g_gearleft_latest == 1.0)
            return(1);
        else
            return(0);
    }
    else if (row["simvar"] == "SIMMIL_GEAR_RIGHT_DOWN")
    {
        if (g_gearright_latest == 1.0)
            return(1);
        else
            return(0);
    }
    else if (row["simvar"] == "SIMMIL_GEAR_LEFT_UP")
    {
        if (g_gearleft_latest > 0.0 && g_gearleft_latest < 1.0)
            return(1);
        else
            return(0);
    }
    else if (row["simvar"] == "SIMMIL_GEAR_RIGHT_UP")
    {
        if (g_gearright_latest > 0.0 && g_gearright_latest < 1.0)
            return(1);
        else
            return(0);
    }

    if (row["p3d_idx"] < 0)
        return(0);

    return(int(row["p3d_latest"]));
}

void P3D_SetSwitch(json row, INT32 pinval)
{
    HRESULT hr;
    double p3d_latest;
    char debug_buffer[DEBUG_MSG_SIZE];

//    fprintf(g_UDPinfo.fpLog, "Set Switch pinval=%d simvar='%s'\n", pinval, g_input_config[config_idx].simvar);


    if (row["p3d_event"] == "SIMMIL_FLAPS")
    {
        if (pinval == 0)
            hr = SimConnect_TransmitClientEvent(g_hSimConnect, 0, EVENT_ID_FLAPS_UP, 0, INPUT0, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);
        else
            hr = SimConnect_TransmitClientEvent(g_hSimConnect, 0, EVENT_ID_FLAPS_DOWN, 0, INPUT0, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);

        return;
    }

    if (row["p3d_event"] == "SIMMIL_GEAR")
    {
        if (pinval == 0)
            hr = SimConnect_TransmitClientEvent(g_hSimConnect, 0, EVENT_ID_GEAR_UP, 0, INPUT0, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);
        else
            hr = SimConnect_TransmitClientEvent(g_hSimConnect, 0, EVENT_ID_GEAR_DOWN, 0, INPUT0, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);

        return;
    }

    if (row["p3d_event"] == "SIMMIL_AXIS_BRAKES")
    {
        DWORD dwData = (DWORD)pinval;
                       hr = SimConnect_TransmitClientEvent(g_hSimConnect, 0, EVENT_ID_AXIS_LEFT_BRAKE_SET, dwData, INPUT0, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);
        hr = SimConnect_TransmitClientEvent(g_hSimConnect, 0, EVENT_ID_AXIS_RIGHT_BRAKE_SET, dwData, INPUT0, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);

        return;
    }

    //Retrieve current value from SIMVAR, if specified:
    if (row["p3d_idx"] >= 0)
    {
        p3d_latest = row["p3d_latest"];
    }
    else
    {
        p3d_latest = -1.0;
    }

    if (row["type"] == "TOG_DREF")
    {
/*
        if (config_idx == 1)
        {
            printf("Switch[%d]: %f %f %s\n", g_input_config[config_idx].pin, p3d_latest, double(pinval), g_input_config[config_idx].simvar);
            return;
        }
*/
        if (p3d_latest == 1.0)
        {
            if(pinval == 0)
                hr = SimConnect_SetDataOnSimObject(g_hSimConnect, row["p3d_input_idx"], SIMCONNECT_OBJECT_ID_USER, 0, 0, sizeof(pinval), &pinval);
        }
        else if (p3d_latest == 0.0)
        {
            if (pinval == 1)
                hr = SimConnect_SetDataOnSimObject(g_hSimConnect, row["p3d_input_idx"], SIMCONNECT_OBJECT_ID_USER, 0, 0, sizeof(pinval), &pinval);
        }

    }
    else if (row["type"] == "TOG_CMD")
    {
        if (p3d_latest == 1.0)
        {
            if (pinval == 0)
            {
                sprintf_s(debug_buffer, sizeof(debug_buffer), "Transmitting %s config_idx %d p3d_latest = %f pinval = %d\n", row["p3d_event"], (int)row["p3d_input_idx"], p3d_latest, pinval);
                my_log(debug_buffer, g_log_dest);

                hr = SimConnect_TransmitClientEvent(g_hSimConnect, 0, row["p3d_input_idx"], 0, INPUT0, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);
            }
        }
        else if (p3d_latest == 0.0)
        {
            if (pinval == 1)
            {
                sprintf_s(debug_buffer, sizeof(debug_buffer), "Transmitting %s config_idx %d p3d_latest = %f pinval = %d\n", row["p3d_event"], (int)row["p3d_input_idx"], p3d_latest, pinval);
                my_log(debug_buffer, g_log_dest);

                hr = SimConnect_TransmitClientEvent(g_hSimConnect, 0, row["p3d_input_idx"], 0, INPUT0, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);
            }
        }
        else if (p3d_latest == -1.0)
        {
            //This is a command without a SIMVAR. Simply do it every time the pin is at 1:
            if (pinval == 1)
            {
                sprintf_s(debug_buffer, sizeof(debug_buffer), "Transmitting %s config_idx %d p3d_latest = %f pinval = %d\n", row["p3d_event"], row["p3d_input_idx"], p3d_latest, pinval);
                my_log(debug_buffer, g_log_dest);

                hr = SimConnect_TransmitClientEvent(g_hSimConnect, 0, row["p3d_input_idx"], 0, INPUT0, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);
            }
        }
    }
    else if (row["type"] == "POT_AXIS")
    {
        DWORD dwData = (DWORD)pinval;
        hr = SimConnect_TransmitClientEvent(g_hSimConnect, 0, row["p3d_input_idx"], dwData, INPUT0, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);
    }

    return;
}

void set_switches(unsigned char *UDPmessage)
{
    int pinnum, boardnum, item;
    INT32 pinval = 0;

    //Find the board in g_config:
    boardnum = int(UDPmessage[0]);
    int msg_len = int(UDPmessage[2]);
    int num_switches = msg_len / 4;

    if (num_switches <= 0)
        return;

    for (json::iterator switches = g_config["switch_boards"].begin(); switches != g_config["switch_boards"].end(); ++switches)
    {
        if ((*switches)["board_id"] != boardnum)
            continue;

        //How many switches are present?:
        int num_rows = (*switches)["rows"].size();

        int msg_len = int(UDPmessage[2]);
        int num_switches = msg_len / 4;
        unsigned int data1, data2;

        for (int i = 0; i < num_switches; i++)
        {
            pinnum = UDPmessage[3 + i * 4];
            data1 = UDPmessage[5 + i * 4];
            data2 = UDPmessage[6 + i * 4];
            pinval = UDPmessage[4 + i * 4];

            //Find it in the rows:
            for (json::iterator row = (*switches)["rows"].begin(); row != (*switches)["rows"].end(); ++row)
            {
                std::string row_str = (*row)["id"];
                int row_id = std::stoi(row_str);
                if (row_id != pinnum)
                    continue;

                std::string driver_type = (*row)["type"];

                if (driver_type == "POT_AXIS")
                {
                    pinval = data1 * 0x100 + data2;

                    if ((*row)["clb"].empty())
                    {
                        //This will never happen through the configurator. Make a guess:
                        double dbl_calc_value = pinval * 32.0295 - 16383.0;
                        pinval = (INT32)dbl_calc_value;
                    }
                    else
                    {
                        pinval = DoCalibrationFromFile((*row)["clb"], double(pinval), 91.01667);
                    }
                }
                else
                {
                    pinval = data1;
                    if ((*row)["polarity"] != 0.0)
                    {
                        if (pinval == 0)
                            pinval = 1;
                        else
                            pinval = 0;
                    }
                }
                P3D_SetSwitch((*row), pinval);
                break;
            }
        }
    }
}

/*
void get_esp32_messages(void)
{
    int ops, status;
    char USBmessage[MAX_USB_LEN];
    char USBmessage_switches[MAX_USB_LEN];
    uint8_t USBm[MAX_USB_LEN];
    bool got_switch = false;
    char debug_buffer[500];

    //Fetch messages, with a max to prevent getting stuck:
    for (ops = 0; ops < MSG_PER_CYCLE; ops++)
    {
        status = GetUDPMessage(&g_UDPinfo, USBm, sizeof(USBm));

        if (status <= 0)
            break;

        if (USBm[1] == UDP_SWITCH_ID)
        {
            if (g_verbose)
            {
                sprintf_s(debug_buffer, sizeof(debug_buffer), "Got Switch Message:");
                my_log(debug_buffer, g_log_dest);
                for (int i = 0; i < (USBm[2] + 3); i++)
                {
                    sprintf_s(debug_buffer, sizeof(debug_buffer), " %02u", (unsigned char)USBm[i]);
                    my_log(debug_buffer, g_log_dest);
                }
                sprintf_s(debug_buffer, sizeof(debug_buffer), "\n");
                my_log(debug_buffer, g_log_dest);
            }
            else
            {
                sprintf_s(debug_buffer, sizeof(debug_buffer), "Got Switch Message for board: %u\n", USBm[0]);
                my_log(debug_buffer, g_log_dest);
            }

            set_switches(USBm);
        }
        else
        {
            sprintf_s(debug_buffer, sizeof(debug_buffer), "Unrecognised UDP message: ");
            my_log(debug_buffer, g_log_dest);
            for (int ii = 0; ii < status; ii++)
            {
                sprintf_s(debug_buffer, sizeof(debug_buffer), " %02u", USBm[ii]);
                my_log(debug_buffer, g_log_dest);
            }
            sprintf_s(debug_buffer, sizeof(debug_buffer), "\n");
            my_log(debug_buffer, g_log_dest);
        }
    }

    return;
}
*/

int send_lights(void)
{
    int status = 0, UDPmessageLen, ival;
    uint8_t UDPmessage[MAX_UDP_LEN];
//    char UDPmessage_switches[MAX_UDP_LEN];
    char debug_buffer[500];

    //Loop through light boards:
    for (json::iterator lights = g_config["light_boards"].begin(); lights != g_config["light_boards"].end(); ++lights)
    {

        UDPmessage[0] = BOARD_LIGHTS + int((*lights)["board_id"]);
        UDPmessage[1] = UDP_LIGHT_ID;
        UDPmessage[2] = 0;        //This is the number of lights
        UDPmessageLen = 3;                       //Use this a running counter

        for (json::iterator row = (*lights)["rows"].begin(); row != (*lights)["rows"].end(); ++row)
        {
            //build the USBMessage:
//            printf("Light ID: %s\n", (*row)["id"]);
            std::string name = (*row)["name"];
            if (name != "")
            {
                std::string row_str = (*row)["id"];
                int row_id = std::stoi(row_str);

                UDPmessage[UDPmessageLen] = uint8_t(row_id);
                UDPmessageLen++;

                if (g_calibrating)
                {
                    ival = 1;
                }
                else
                {
                    ival = P3D_GetLightData(*row);
                }

                if (ival)
                    UDPmessage[UDPmessageLen] = 1;
                else
                    UDPmessage[UDPmessageLen] = 0;

                sprintf_s(debug_buffer, sizeof(debug_buffer), "Light %d %s value: %d\n", row_id, name.c_str(), ival);
                my_log(debug_buffer, g_log_dest);

                UDPmessageLen++;
            }
        }
        //Set message length:
        UDPmessage[2] = UDPmessageLen - 3;

        //Send the completed UDP message for this board:
        if (g_verbose)
        {
            sprintf_s(debug_buffer, sizeof(debug_buffer), "Send Lights Message:");
            my_log(debug_buffer, g_log_dest);
            for (int i = 0; i < UDPmessageLen; i++)
            {
                sprintf_s(debug_buffer, sizeof(debug_buffer), " %02u", (unsigned char)UDPmessage[i]);
                my_log(debug_buffer, g_log_dest);
            }
            sprintf_s(debug_buffer, sizeof(debug_buffer), "\n");
            my_log(debug_buffer, g_log_dest);
        }

        status = SendUDPMessage(&g_UDPconnect[0], UDPmessage, UDPmessageLen);
    }
    return status;
}

int send_switches(void)
{
    int status = 0, UDPmessageLen;
    uint8_t UDPmessage[MAX_USB_LEN];
    char debug_buffer[500];

    //Loop through switch boards:
    for (json::iterator switches = g_config["switch_boards"].begin(); switches != g_config["switch_boards"].end(); ++switches)
    {

        UDPmessage[0] = BOARD_SWITCHES + int((*switches)["board_id"]);
        UDPmessage[1] = UDP_SWITCH_ID;
        UDPmessage[2] = 0;        //This is the number of switches
        UDPmessageLen = 3;                       //Use this a running counter

        for (json::iterator row = (*switches)["rows"].begin(); row != (*switches)["rows"].end(); ++row)
        {
            //build the USBMessage:
            std::string name = (*row)["name"];
            if (name != "")
            {
                std::string row_str = (*row)["id"];
                if (row_str.at(0) == 'A')
                    row_str.at(0) = '1';

                int row_id = std::stoi(row_str);
                UDPmessage[UDPmessageLen] = uint8_t(row_id);
                UDPmessageLen++;

                std::string type = (*row)["type"];
                uint8_t type_id = 0;

                if (type == "TOG_DREF")
                    type_id = SW_TOG_DREF;
                else if (type == "MOM_DREF")
                    type_id = SW_MOM_DREF;
                else if (type == "TOG_CMD")
                    type_id = SW_TOG_CMD;
                else if (type == "MOM_CMD")
                    type_id = SW_MOM_CMD;
                else if (type == "ROT_CMD_UP")
                    type_id = SW_ROT_CMD_UP;
                else if (type == "ROT_CMD_DWN")
                    type_id = SW_ROT_CMD_DWN;
                else if (type == "POT_AXIS")
                    type_id = SW_POT_AXIS;

                UDPmessage[UDPmessageLen++] = type_id;

                //Send two empty slots for message data - just to keep the message format the same both ways:
                UDPmessage[UDPmessageLen++] = 0;
                UDPmessage[UDPmessageLen++] = 0;
            }
        }            

        //Set message length:
        UDPmessage[2] = UDPmessageLen - 3;

        //Send the completed UDP message for this board:
        if (g_verbose)
        {
            sprintf_s(debug_buffer, sizeof(debug_buffer), "Send Switches Message:");
            my_log(debug_buffer, g_log_dest);
            for (int i = 0; i < UDPmessageLen; i++)
            {
                sprintf_s(debug_buffer, sizeof(debug_buffer), " %02u", (unsigned char)UDPmessage[i]);
                my_log(debug_buffer, g_log_dest);
            }
            sprintf_s(debug_buffer, sizeof(debug_buffer), "\n");
            my_log(debug_buffer, g_log_dest);
        }

        status = SendUDPMessage(&g_UDPconnect[0], UDPmessage, UDPmessageLen);
    }
    return status;
}

int send_A3967(void)
{
    int status = 0, UDPmessageLen;
    uint8_t UDPmessage[MAX_UDP_LEN];
    double dval;
    float fval;
    int iclb_out, i;
    char byte01 = NULL, byte02 = NULL;
    char debug_buffer[500];
    
    for (json::iterator a3967 = g_config["a3967_boards"].begin(); a3967 != g_config["a3967_boards"].end(); ++a3967)
    {

        UDPmessage[0] = BOARD_A3967 + int((*a3967)["board_id"]);
        UDPmessage[1] = UDP_A3967_ID;
        UDPmessage[2] = 0;        //This is the number of A3967 steppers
        UDPmessageLen = 3;                       //Use this a running counter

        for (json::iterator row = (*a3967)["rows"].begin(); row != (*a3967)["rows"].end(); ++row)
        {
            //Get the data:
            if (g_calibrating)
            {
                dval = double((*row)["calib"]);
            }
            else
            {
                dval = P3D_GetDoubleData(*row);
            }

            if ((*row)["clb"].empty())
            {
                //We should never get here. Guess::
                //This driver takes 12 steps per degree:
                iclb_out = int(dval);
            }
            else
            {
                //Do the conversion from the calibration file:
                iclb_out = DoCalibrationFromFile((*row)["clb"], dval, 12.0);
            }
            if (iclb_out < 0)
            {
                iclb_out = 0;
            }

            if ((*row)["clb"].empty())
            {
                //Shouldn't get here. Make a wild guess:
                iclb_out = int(dval);
            }
            else
            {
                //Do the calibration from the file:
                iclb_out = DoCalibrationFromFile((*row)["clb"], dval, double(A3967_FULL_CIRCLE) / 360.0);
            }

            //These gauges can go round 360 degrees, so if the value becomes -ve, add a full circle to it:
            if (iclb_out < 0 && (*row)["driver"] == "360")
            {
                iclb_out += A3967_FULL_CIRCLE;
            }

            if (iclb_out < 0 && (*row)["driver"] == "normal")
            {
                iclb_out = 0;
            }

            std::string name = (*row)["name"];
            std::string row_str = (*row)["id"];
            int row_id = std::stoi(row_str);

            sprintf_s(debug_buffer, sizeof(debug_buffer), "A3967 %d %s value: %f  sent: %f degrees\n", row_id, name.c_str(), dval, double(iclb_out) / double(A3967_FULL_CIRCLE) * 360.0);
            my_log(debug_buffer, g_log_dest);
            byte01 = char(iclb_out / 0x100);
            byte02 = iclb_out - byte01 * 0x100;

            UDPmessage[UDPmessageLen++] = uint8_t(row_id);

            if ((*row)["driver"] == "360")
                UDPmessage[UDPmessageLen++] = 1;
            else
                UDPmessage[UDPmessageLen++] = 0;
            UDPmessage[UDPmessageLen++] = byte01;
            UDPmessage[UDPmessageLen++] = byte02;
        }

        //Set message length:
        UDPmessage[2] = UDPmessageLen - 3;

        //Send the completed UDP message for this board:
        if (g_verbose)
        {
            sprintf_s(debug_buffer, sizeof(debug_buffer), "Send A3967 Message:");
            my_log(debug_buffer, g_log_dest);
            for (i = 0; i < UDPmessageLen; i++)
            {
                sprintf_s(debug_buffer, sizeof(debug_buffer), " %02u", (unsigned char)UDPmessage[i]);
                my_log(debug_buffer, g_log_dest);
            }
            sprintf_s(debug_buffer, sizeof(debug_buffer), "\n");
            my_log(debug_buffer, g_log_dest);
        }

        status = SendUDPMessage(&g_UDPconnect[0], UDPmessage, UDPmessageLen);
    }

    return status;
}

int send_VID6606(void)
{
    int status = 0, UDPmessageLen;
    uint8_t UDPmessage[MAX_UDP_LEN];
    double dval;
    float fval;
    int iclb_out, i;
    char byte01 = NULL, byte02 = NULL;
    char debug_buffer[500];

    for (json::iterator vid6606 = g_config["vid6606_boards"].begin(); vid6606 != g_config["vid6606_boards"].end(); ++vid6606)
    {

        UDPmessage[0] = BOARD_VID6606 + int((*vid6606)["board_id"]);
        UDPmessage[1] = UDP_VID6606_ID;
        UDPmessage[2] = 0;        //This is the number of VID6606 steppers
        UDPmessageLen = 3;                       //Use this a running counter

        for (json::iterator row = (*vid6606)["rows"].begin(); row != (*vid6606)["rows"].end(); ++row)
        {
            //Get the data:
            if (g_calibrating)
            {
                dval = double((*row)["calib"]);
            }
            else
            {
                dval = P3D_GetDoubleData(*row);
            }

            if ((*row)["clb"].empty())
            {
                //We should never get here. Guess::
                //This driver takes 12 steps per degree:
                iclb_out = int(dval);
            }
            else
            {
                //Do the conversion from the calibration file:
                iclb_out = DoCalibrationFromFile((*row)["clb"], dval, 12.0);
            }
            if (iclb_out < 0)
            {
                iclb_out = 0;
            }

            std::string name = (*row)["name"];
            std::string row_str = (*row)["id"];
            int row_id = std::stoi(row_str);
            sprintf_s(debug_buffer, sizeof(debug_buffer), "VID6606 %d %s value: %f  sent: %f degrees\n", row_id, name.c_str(), dval, float(iclb_out) / 12.0);
            my_log(debug_buffer, g_log_dest);
            byte01 = char(iclb_out / 0x100);
            byte02 = iclb_out - byte01 * 0x100;

            UDPmessage[UDPmessageLen] = uint8_t(row_id);
            UDPmessage[UDPmessageLen + 1] = byte01;
            UDPmessage[UDPmessageLen + 2] = byte02;
            UDPmessageLen += 3;
        }

        //Set message length:
        UDPmessage[2] = UDPmessageLen - 3;

        //Send the completed USB message for this board:
        if (g_verbose)
        {
            sprintf_s(debug_buffer, sizeof(debug_buffer), "Send VID6606 Message:");
            my_log(debug_buffer, g_log_dest);
            for (i = 0; i < UDPmessageLen; i++)
            {
                sprintf_s(debug_buffer, sizeof(debug_buffer), " %02u", (unsigned char)UDPmessage[i]);
                my_log(debug_buffer, g_log_dest);
            }
            sprintf_s(debug_buffer, sizeof(debug_buffer), "\n");
            my_log(debug_buffer, g_log_dest);
        }

        status = SendUDPMessage(&g_UDPconnect[0], UDPmessage, UDPmessageLen);
    }

    return status;
}

int P3DFlightLoop()
{
    int  status = 0, ival, pinnum, pinval, item, USBmessageLen = 0;
    uint8_t UDPmessage[MAX_UDP_LEN], byte01=NULL, byte02=NULL;
    double dval;
    float fval;
    int iclb_out, i;
    int num_VID6606 = 0;
    static bool do_VID6606 = false;
    char debug_buffer[500];

    //Check port status:
    if (g_UDPSocketStatus != UDP_SOCKET_CONNECTED)
    {
        int ret = SetupSocket();

        //Set up UDP connections for two esp32:
        strcpy_s(g_UDPconnect[0].target_ip, sizeof(g_UDPconnect[0].target_ip), "192.168.138.2");
        SetupConnection(&g_UDPconnect[0]);
        strcpy_s(g_UDPconnect[1].target_ip, sizeof(g_UDPconnect[0].target_ip), "192.168.138.3");
        SetupConnection(&g_UDPconnect[1]);

        Sleep(10);
        //    strcpy_s(g_UDPconnect[1].target_ip, sizeof(g_UDPconnect[0].target_ip), "192.168.138.3");
        //    SetupConnection(&g_UDPconnect[1]);

        if (g_UDPSocketStatus != UDP_SOCKET_CONNECTED)
        {
            return P3D_WAIT_TIME;
        }
    }

    //Get the Mega messages and process them.
    //The routine returns if it got switches, so we can make sure our connection is alive.
//    get_esp32_messages();

    //The loopback alternates between doing lights/A3967 and switches/VID6606:
    if (!do_VID6606)
    {
        status = send_lights();
        status = send_switches();

        //If the UDP message failed, it will have closed the USB connection, so we'll have to try to
        //reconnect by starting again immediately:
        if (status  <  0)
            return 0;

        status = send_A3967();

        do_VID6606 = true;
    }
    else
    {
        //This is the do_VID6606 Section of code, which alternates with the code above.
        status = send_VID6606();

        do_VID6606 = false;
    }

    //This will cause the callback to be called again in 25 millseconds:
    //It alternates between lights+A3967s and Switches+VID6606s.
    return P3D_WAIT_TIME;
}

void CALLBACK MyDispatchProcRD(SIMCONNECT_RECV* pData, DWORD cbData, void *pContext)
{
    HRESULT hr;
    
    switch(pData->dwID)
    {
        case SIMCONNECT_RECV_ID_EVENT:
        {
            SIMCONNECT_RECV_EVENT *evt = (SIMCONNECT_RECV_EVENT*)pData;

            switch (evt->uEventID)
            {
                case EVENT_ID_SIM_START:
                {
                    // Give the plugin loop the green light:
                    g_simstarted = true;
                }
                break;

                case EVENT_ID_START_CALIBRATION:
                {
                    my_log("Start Calibrating!\n", g_log_dest);
                    g_calibrating = 1;

                    hr = SimConnect_MenuDeleteItem(g_hSimConnect, EVENT_ID_START_CALIBRATION);
                    hr = SimConnect_MenuAddItem(g_hSimConnect, STOP_STRING, EVENT_ID_STOP_CALIBRATION, 0);

                    break;
                }

                case EVENT_ID_STOP_CALIBRATION:
                {
                    my_log("Stop Calibrating!\n", g_log_dest);
                    g_calibrating = 0;

                    hr = SimConnect_MenuDeleteItem(g_hSimConnect, EVENT_ID_STOP_CALIBRATION);
                    hr = SimConnect_MenuAddItem(g_hSimConnect, START_STRING, EVENT_ID_START_CALIBRATION, 0);

                    break;
                }
/*
                case EVENT_ID_RELOAD_CONFIG:
                {
                    int status = ReadConfigFile();
                    SendConnectionRequest(&g_UDPinfo);
                    break;
                }
                case EVENT_ID_RESET_MAIN_MEGA:
                {
                    SendArduinoReset(&g_UDPinfo, 0);
                    break;
                }
*/
                default:
                {
                    break;
                }
            }

            break; // SIMCONNECT_RECV_ID_EVENT
        }

        case SIMCONNECT_RECV_ID_SIMOBJECT_DATA:
        {
            SIMCONNECT_RECV_SIMOBJECT_DATA* pObjData = (SIMCONNECT_RECV_SIMOBJECT_DATA*)pData;

            switch(pObjData->dwRequestID)
            {
                case REQUEST_ID_USER_OBJECT_DATA:
                {
                    DWORD dwObjectID = pObjData->dwObjectID;

                    double *p3dData = (double *)&pObjData->dwData;

                    //Start with sim time:
                    g_latest_sim_time =  *(p3dData);

                    //Variables used by SIMMIL_         
                    g_altitude_latest = *(p3dData + g_altitude_data_idx);
                    g_chassis_latest = *(p3dData + g_chassis_idx);
                    g_gearleft_latest = *(p3dData + g_gearleft_idx);
                    g_gearright_latest = *(p3dData + g_gearright_idx);

                    //Assign all the vid6606:
                    for (json::iterator vid6606 = g_config["vid6606_boards"].begin(); vid6606 != g_config["vid6606_boards"].end(); ++vid6606) 
                    {
                        for (json::iterator row = (*vid6606)["rows"].begin(); row != (*vid6606)["rows"].end(); ++row) 
                        {
                            int p3d_idx = int((*row)["p3d_idx"]);
                            if (p3d_idx >= 0)
                            {
                               (*row)["p3d_latest"] = *(p3dData + p3d_idx);
                            }
                        }
                    }

                    //Assign all the a3967:
                    for (json::iterator a3967 = g_config["a3967_boards"].begin(); a3967 != g_config["a3967_boards"].end(); ++a3967)
                    {
                        for (json::iterator row = (*a3967)["rows"].begin(); row != (*a3967)["rows"].end(); ++row)
                        {
                            int p3d_idx = int((*row)["p3d_idx"]);
                            if (p3d_idx >= 0)
                            {
                                (*row)["p3d_latest"] = *(p3dData + p3d_idx);
                            }
                        }
                    }

                    //Assign all the lights:
                    for (json::iterator lights = g_config["light_boards"].begin(); lights != g_config["light_boards"].end(); ++lights)
                    {
                        for (json::iterator row = (*lights)["rows"].begin(); row != (*lights)["rows"].end(); ++row)
                        {
                            int p3d_idx = int((*row)["p3d_idx"]);
                            if (p3d_idx >= 0)
                            {
                                (*row)["p3d_latest"] = *(p3dData + p3d_idx);
                            }
                        }
                    }

                    //Assign all the switches with simvar:
                    for (json::iterator switches = g_config["switch_boards"].begin(); switches != g_config["switch_boards"].end(); ++switches)
                    {
                        for (json::iterator row = (*switches)["rows"].begin(); row != (*switches)["rows"].end(); ++row)
                        {
                            int p3d_idx = int((*row)["p3d_idx"]);
                            if (p3d_idx >= 0)
                            {
                                (*row)["p3d_latest"] = *(p3dData + p3d_idx);
                            }
                        }
                    }

                    break;
                }
                default:
                {
                    break;
                }
            }

            break; // SIMCONNECT_RECV_ID_SIMOBJECT_DATA
        }
        
        case SIMCONNECT_RECV_ID_QUIT:
        {
            g_bQuit = 1;
            break;
        }

        default:
        {
            printf("\nReceived: %d", pData->dwID);
            break;
        }
    }
}

void RunSimmilP3D()
{
    HRESULT hr;
    
    bool bConnected = false;

    // Attempt to connect to Prepar3D.
    my_log("Attempting to connect to Prepar3D...\n", g_log_dest);
    for (unsigned int i = 0; i < 60; ++i)
    {
        if (SUCCEEDED(SimConnect_Open(&g_hSimConnect, TITLE_STRING, NULL, 0, 0, 0)))
        {
            bConnected = true;
            break;
        }

        printf("\nAttempt %d", i + 1);
        Sleep(1000);
    }

    if (bConnected)
    {
        my_log("Connected to Prepar3D!\n", g_log_dest);

        //Start with the time:
        hr = SimConnect_AddToDataDefinition(g_hSimConnect, DEFINITION_ID_USER_OBJECT, "SIM TIME", "Seconds");
        g_p3dData_num++;

        //Now all the variables that SIMMIL_ needs:
        hr = SimConnect_AddToDataDefinition(g_hSimConnect, DEFINITION_ID_USER_OBJECT, "INDICATED ALTITUDE", "Feet");
        g_altitude_data_idx = g_p3dData_num;
        g_p3dData_num++;

        hr = SimConnect_AddToDataDefinition(g_hSimConnect, DEFINITION_ID_USER_OBJECT, "GEAR POSITION", "");
        g_chassis_idx = g_p3dData_num;
        g_p3dData_num++;

        hr = SimConnect_AddToDataDefinition(g_hSimConnect, DEFINITION_ID_USER_OBJECT, "GEAR LEFT POSITION", "");
        g_gearleft_idx = g_p3dData_num;
        g_p3dData_num++;

        hr = SimConnect_AddToDataDefinition(g_hSimConnect, DEFINITION_ID_USER_OBJECT, "GEAR RIGHT POSITION", "");
        g_gearright_idx = g_p3dData_num;
        g_p3dData_num++;

        // Request a simulation start event
        hr = SimConnect_SubscribeToSystemEvent(g_hSimConnect, EVENT_ID_SIM_START, "SimStart");

        //Set up commands for SIMMIL_ switches:
        hr = SimConnect_MapClientEventToSimEvent(g_hSimConnect, EVENT_ID_GEAR_UP, "GEAR_UP");
        hr = SimConnect_MapClientEventToSimEvent(g_hSimConnect, EVENT_ID_GEAR_DOWN, "GEAR_DOWN");
        hr = SimConnect_MapClientEventToSimEvent(g_hSimConnect, EVENT_ID_AXIS_LEFT_BRAKE_SET, "AXIS_LEFT_BRAKE_SET");
        hr = SimConnect_MapClientEventToSimEvent(g_hSimConnect, EVENT_ID_AXIS_RIGHT_BRAKE_SET, "AXIS_RIGHT_BRAKE_SET");

        //Set up definitions for VID6606 boards:
        for (json::iterator vid6606 = g_config["vid6606_boards"].begin(); vid6606 != g_config["vid6606_boards"].end(); ++vid6606) 
        {
            for (json::iterator rows = (*vid6606)["rows"].begin(); rows != (*vid6606)["rows"].end(); ++rows) 
            {
                //Add a new fields:
                (*rows).emplace("p3d_idx", -1);
                (*rows).emplace("p3d_latest", double(0.0));

                std::string simvar = (*rows)["p3d_simvar"];
                std::string p3d_units = (*rows)["p3d_units"];

                if (simvar == "")
                    continue;

                if (simvar.substr(0,7) != "SIMMIL_")
                {
                    hr = SimConnect_AddToDataDefinition(g_hSimConnect, DEFINITION_ID_USER_OBJECT, simvar.c_str(), p3d_units.c_str());
                    (*rows)["p3d_idx"] = g_p3dData_num;
                    g_p3dData_num++;
                }
            }
        }

        //Set up definitions A3967 boards:
        for (json::iterator a3967 = g_config["a3967_boards"].begin(); a3967 != g_config["a3967_boards"].end(); ++a3967)
        {
            for (json::iterator rows = (*a3967)["rows"].begin(); rows != (*a3967)["rows"].end(); ++rows)
            {
                //Add a new fields:
                (*rows).emplace("p3d_idx", -1);
                (*rows).emplace("p3d_latest", double(0.0));

                std::string simvar = (*rows)["p3d_simvar"];
                std::string p3d_units = (*rows)["p3d_units"];

                if (simvar == "")
                    continue;

                if (simvar.substr(0, 7) != "SIMMIL_")
                {
                    hr = SimConnect_AddToDataDefinition(g_hSimConnect, DEFINITION_ID_USER_OBJECT, simvar.c_str(), p3d_units.c_str());
                    (*rows)["p3d_idx"] = g_p3dData_num;
                    g_p3dData_num++;
                }
            }
        }

        //Set up Light boards boards:
        for (json::iterator lights = g_config["light_boards"].begin(); lights != g_config["light_boards"].end(); ++lights)
        {
            for (json::iterator rows = (*lights)["rows"].begin(); rows != (*lights)["rows"].end(); ++rows)
            {
                //Add a new fields:
                (*rows).emplace("p3d_idx", -1);
                (*rows).emplace("p3d_latest", double(0.0));

                std::string simvar = (*rows)["p3d_simvar"];

                if (simvar == "")
                    continue;

                if (simvar.substr(0, 7) != "SIMMIL_")
                {
                    hr = SimConnect_AddToDataDefinition(g_hSimConnect, DEFINITION_ID_USER_OBJECT, simvar.c_str(), "");
                    (*rows)["p3d_idx"] = g_p3dData_num;
                    g_p3dData_num++;
                }
            }
        }

        //Set up Switches:
        int input_idx = 0;
        for (json::iterator switches = g_config["switch_boards"].begin(); switches != g_config["switch_boards"].end(); ++switches)
        {
            for (json::iterator rows = (*switches)["rows"].begin(); rows != (*switches)["rows"].end(); ++rows)
            {
                //Add a new fields:
                (*rows).emplace("p3d_idx", -1);
                (*rows).emplace("p3d_latest", double(0.0));
                (*rows).emplace("p3d_input_idx", double(0.0));

                std::string simvar = (*rows)["p3d_simvar"];
                std::string p3d_event = (*rows)["p3d_event"];
                std::string p3d_units = (*rows)["p3d_units"];
                std::string type = (*rows)["type"];

                if (simvar.substr(0, 7) != "SIMMIL_" && p3d_event.substr(0, 7) != "SIMMIL_")
                {
                    //First we add the definition to the stream of data coming in:
                    if (simvar != "")
                    {
                        hr = SimConnect_AddToDataDefinition(g_hSimConnect, DEFINITION_ID_USER_OBJECT, simvar.c_str(), NULL);
                        (*rows)["p3d_idx"] = g_p3dData_num;
                        g_p3dData_num++;
                    }

                    if (type == "TOG_CMD" ||
                        type == "POT_AXIS")
                    {
                        //Now we create a special event id for setting this variable, using the index i as the EVENT_ID
                        //The deal is that P3D usually uses ENUM for these, which is only to assign each a unique number. I've assigned all my other event ids 
                        //to > 90, so all the idx figures in the g_input_config will be unique EVENT_ID:
                        hr = SimConnect_MapClientEventToSimEvent(g_hSimConnect, input_idx, p3d_event.c_str());
                        (*rows)["p3d_input_idx"] = input_idx;
                        input_idx++;
                    }
                    else if (type == "TOG_DREF")
                    {
                        //Now we create a special data definition for setting this variable, using the index i as the DEFINITION_ID
                        //The deal is that P3D usually uses ENUM for these, which is only to assign each a unique number. I've assigned DEFINITION_ID_USER_OBJECT
                        //to 99, so all the idx figures in the g_input_config will be unique DEFINITION_ID:
                        hr = SimConnect_AddToDataDefinition(g_hSimConnect, input_idx, simvar.c_str(), NULL, SIMCONNECT_DATATYPE_INT32);
                        (*rows)["p3d_input_idx"] = input_idx;
                        input_idx++;
                    }
                }
            }
        }

        hr = SimConnect_SetInputGroupState(g_hSimConnect, INPUT0, SIMCONNECT_STATE_ON);

        //Add the calibration menu iteme:
        hr = SimConnect_MenuAddItem(g_hSimConnect, START_STRING, EVENT_ID_START_CALIBRATION, 0);
//        hr = SimConnect_MenuAddItem(g_hSimConnect, RELOAD_CONFIG_STRING, EVENT_ID_RELOAD_CONFIG, 0);
//        hr = SimConnect_MenuAddItem(g_hSimConnect, RESET_MAIN_ARDUINO_STRING, EVENT_ID_RESET_MAIN_MEGA, 0);


        //Start the data flow:
        my_log("Starting SimmilPluginP Dataflow...\n", g_log_dest);
        hr = SimConnect_RequestDataOnSimObject(g_hSimConnect, REQUEST_ID_USER_OBJECT_DATA, DEFINITION_ID_USER_OBJECT, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_SIM_FRAME);

        // Application main loop.
        int wait_time = P3D_WAIT_TIME;
        while (0 == g_bQuit)
        {
            SimConnect_CallDispatch(g_hSimConnect, MyDispatchProcRD, NULL);

            if(g_simstarted)
                wait_time = P3DFlightLoop();
            
            //Sleep for 25 msec:
            Sleep(wait_time);
        }

        my_log("Quit Flight Loop\n", g_log_dest);

        //Stop data flow:
        hr = SimConnect_RequestDataOnSimObject(g_hSimConnect, REQUEST_ID_USER_OBJECT_DATA, DEFINITION_ID_USER_OBJECT, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_NEVER);

        // Clean up menu::
        if (g_calibrating)
        {
            hr = SimConnect_MenuDeleteItem(g_hSimConnect, EVENT_ID_STOP_CALIBRATION);
        }
        else
        {
            hr = SimConnect_MenuDeleteItem(g_hSimConnect, EVENT_ID_START_CALIBRATION);
        }
//        hr = SimConnect_MenuDeleteItem(g_hSimConnect, EVENT_ID_RELOAD_CONFIG);
//        hr = SimConnect_MenuDeleteItem(g_hSimConnect, EVENT_ID_RESET_MAIN_MEGA);

        // Close.
        hr = SimConnect_Close(g_hSimConnect);
    }
    else
    {
        my_log("Connection timeout!\n", g_log_dest);
    }
}

int __cdecl _tmain(int argc, _TCHAR* argv[])
{
    char cwd[200];
    DWORD cwd_len = 200;
    char debug_buffer[500];

    //Set this here so that the ReadUSB thread doesn't try to do anything before the correct connection level is reached:
//    g_UDPSocketStatus = UDP_SOCKET_DISCONNECTED;

    //Report Argument:
    if (argc < 2)
    {
//        printf("Must provide path to .cfg in command line\n");
//        return(-1);
        strcpy_s(g_directory_path, sizeof(g_directory_path), ".");
        strcat_s(g_directory_path, sizeof(g_directory_path), "\\");
    }
    else
    {
        strcpy_s(g_directory_path, sizeof(g_directory_path), argv[1]);
        strcat_s(g_directory_path, sizeof(g_directory_path), "\\");

        //Cudge to run from debugger:
        g_simstarted = true;

    }

    setup_log(g_directory_path);

    int status = ReadConfigFile();

    RunSimmilP3D();

    my_log("Closing Logfile.\n", g_log_dest);
    close_log();

//    ReadUSB_thread.join();
    return 0;
}
