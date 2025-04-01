/*
 * Proof of concept for the 'Geek szitman supercamera' endoscope
 *
 * This endoscope uses the 'com.useeplus.protocol' protocol.
 * Only hardware revision 1.00 was tested.
 *
 * SPDX-License-Identifier: CC0-1.0
 * by hbens
 */

#include <cassert>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>
#include <version>

#ifndef __cpp_lib_format
#include <ctime>
#endif

#include <libusb-1.0/libusb.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#ifdef __clang__
#pragma GCC diagnostic ignored "-Wdeprecated-anon-enum-enum-conversion"
#endif
#include <opencv2/highgui.hpp>
#pragma GCC diagnostic pop

static constexpr int VERBOSE = 0;

#define KRST "\e[0m"
#define KRED "\e[0;31m"
#define KGRN "\e[0;32m"
#define KYLW "\e[0;33m"
#define KBLU "\e[0;34m"
#define KMAJ "\e[0;35m"
#define KCYN "\e[0;36m"

using byteVector = std::vector<uint8_t>;

class UsbSupercamera {
    static constexpr uint16_t USB_VENDOR_ID = 0x2ce3;
    static constexpr uint16_t USB_PRODUCT_ID = 0x3828;
    static constexpr int INTERFACE_A_NUMBER = 0;
    static constexpr int INTERFACE_B_NUMBER = 1;
    static constexpr int INTERFACE_B_ALTERNATE_SETTING = 1;
    static constexpr unsigned char ENDPOINT_1 = 1;
    static constexpr unsigned char ENDPOINT_2 = 2;
    static constexpr unsigned int USB_TIMEOUT = 1000; /* in ms */

    libusb_context *ctx;
    libusb_device_handle *handle;

    int usb_read(unsigned char endpoint, byteVector &buf, size_t max_size, int debug) {
        int ret;
        int transferred;
        buf.resize(max_size);
        ret = libusb_bulk_transfer(handle, LIBUSB_ENDPOINT_IN | endpoint,
                                   buf.data(), buf.size(), &transferred, USB_TIMEOUT);
        if (ret != 0) {
            std::cerr << KRED "USB READ ERROR ("<< ret << ") " << libusb_strerror(ret) << KRST << std::endl;
            buf.resize(0);
            return ret;
        }
        if (debug > 0) {
            std::ostringstream ss;
            ss << KGRN "   IN " << std::setw(5) << transferred;
            if (debug > 1) {
                ss << " << [";
                int max_print = std::min(10, transferred);
                if (debug > 2) {
                    max_print = transferred;
                }
                for (int i=0; i<max_print; i++) {
                    ss << " " << std::setfill('0') << std::setw(2) << std::hex << static_cast<unsigned>(buf[i]);
                }
                if (max_print < transferred) {
                    ss << " ...";
                }
                ss << " ]";
            }
            ss << KRST << std::endl;
            std::cerr << ss.str();
        }
        buf.resize(transferred);
        return 0;
    }

    int usb_write(unsigned char endpoint, byteVector buf, int debug) {
        int ret;
        int transferred;
        ret = libusb_bulk_transfer(handle, LIBUSB_ENDPOINT_OUT | endpoint,
                                   buf.data(), buf.size(), &transferred, USB_TIMEOUT);
        if (ret != 0) {
            std::cerr << KRED "USB WRITE ERROR ("<< ret << ") " << libusb_strerror(ret) << KRST << std::endl;
            return -1;
        }
        if (debug > 0) {
            std::ostringstream ss;
            ss << KYLW "   OUT >> [";
            for (int i=0; i<transferred; i++) {
                ss << " " << std::setfill('0') << std::setw(2) << std::hex << static_cast<unsigned>(buf[i]);
            }
            ss << " ]" KRST << std::endl;
            std::cerr << ss.str();
        }
        return 0;
    }

    int setup() {
        int ret;

        ret = libusb_init(&ctx);
        if (ret < 0) {
            std::cerr << "fatal: libusb_init fail (" << ret << ")" << std::endl;
            return 1;
        }

        handle = libusb_open_device_with_vid_pid(ctx, USB_VENDOR_ID, USB_PRODUCT_ID);
        if (!handle) {
            std::cerr << "fatal: usb device not found" << std::endl;
            return 1;
        }

        ret = libusb_reset_device(handle);
        if (ret < 0) {
            std::cerr << "fatal: libusb_reset_device error (" << ret << ")" << std::endl;
            return 1;
        }

        ret = libusb_claim_interface(handle, INTERFACE_A_NUMBER);
        if (ret < 0) {
            std::cerr << "fatal: usb_claim_interface A error (" << ret << ")" << std::endl;
            return 1;
        }

        ret = libusb_claim_interface(handle, INTERFACE_B_NUMBER);
        if (ret < 0) {
            std::cerr << "fatal: usb_claim_interface B error (" << ret << ")" << std::endl;
            return 1;
        }

        ret = libusb_set_interface_alt_setting(handle, INTERFACE_B_NUMBER, INTERFACE_B_ALTERNATE_SETTING);
        if (ret < 0) {
            std::cerr << "fatal: libusb_set_interface_alt_setting B error (" << ret << ")" << std::endl;
            return 1;
        }

        return 0;
    }

public:
    UsbSupercamera() {
        int ret = setup();
        if (ret != 0) {
            throw 1;
        }
        /* Hey witch doctor, give us the magic words */
        byteVector ep2_buf = {0xFF, 0x55, 0xFF, 0x55, 0xEE, 0x10};
        usb_write(ENDPOINT_2, ep2_buf, VERBOSE);
        byteVector start_stream = {0xBB, 0xAA, 5, 0, 0};
        usb_write(ENDPOINT_1, start_stream, VERBOSE);
    }

    ~UsbSupercamera() {
        libusb_close(handle);
        libusb_exit(ctx);
    }

    int read_frame(byteVector &read_buf) {
        return usb_read(ENDPOINT_1, read_buf, 0x1000, VERBOSE);
    }
};

class UPPCamera {
    /* all packed struct fields are little-endian */
    static_assert(std::endian::native == std::endian::little);

    struct [[gnu::packed]] upp_usb_frame_t {
        uint16_t magic;
        uint8_t cid; /* camera id */
        uint16_t length; /* does not include the 5-bytes header length */
    };

    struct [[gnu::packed]] upp_cam_frame_t {
        uint8_t fid; /* frame id */
        uint8_t cam_num; /* camera number */
        /* misc flags in the next byte */
        unsigned char has_g:1;
        unsigned char button_press:1;
        unsigned char other:6;
        uint32_t g_sensor;
    };

    typedef void(*btn_callback_t)();
    typedef void(*pic_callback_t)(const byteVector &pic);
    pic_callback_t pic_callback;
    btn_callback_t btn_callback;

    static constexpr uint16_t UPP_USB_MAGIC = 0xBBAA;
    static constexpr uint8_t UPP_CAMID_7 = 7;

    byteVector camera_buffer;
    upp_cam_frame_t cam_header = {};

public:
    UPPCamera(pic_callback_t pic_callback, btn_callback_t btn_callback) {
        this->pic_callback = pic_callback;
        this->btn_callback = btn_callback;
    }

    void handle_upp_frame(const byteVector &data) {
        /* Decode upp_usb_frame_t */
        size_t usb_header_len = sizeof(upp_usb_frame_t);
        if (data.size() < usb_header_len) {
            std::cerr << __func__ << " usb frame too small" << std::endl;
            return;
        }
        const upp_usb_frame_t *frame = (upp_usb_frame_t *) data.data();
        if (frame->magic != UPP_USB_MAGIC) {
            std::cerr << __func__ << " usb frame bad magic" << std::endl;
            return;
        }
        if (frame->cid != UPP_CAMID_7) {
            std::cerr << __func__ << " unknown camera ID" << std::endl;
            return;
        }
        if (usb_header_len + frame->length != data.size()) {
            std::cerr << __func__ << " bad usb frame length" << std::endl;
            return;
        }

        /* Decode upp_cam_frame_t */
        size_t cam_header_len = sizeof(upp_cam_frame_t);
        if (data.size() - usb_header_len < cam_header_len) {
            std::cerr << __func__ << " cam frame too small" << std::endl;
            return;
        }
        const upp_cam_frame_t *p_cam_header = (upp_cam_frame_t *) (data.data() + usb_header_len);

        if ((camera_buffer.size() > 0) && (cam_header.fid != p_cam_header->fid)) {
            pic_callback(camera_buffer);
            camera_buffer.resize(0);
        }

        if (camera_buffer.size() == 0) {
            cam_header = *p_cam_header;
            assert(cam_header.cam_num < 2);
            assert(cam_header.has_g == 0);
            assert(cam_header.other == 0);
        } else {
            assert(cam_header.fid == p_cam_header->fid);
            assert(cam_header.cam_num == p_cam_header->cam_num);
            assert(cam_header.has_g == p_cam_header->has_g);
            assert(cam_header.other == p_cam_header->other);
        }
        if (p_cam_header->button_press) {
            btn_callback();
        }

        auto data_start = data.begin() + usb_header_len + cam_header_len;
        camera_buffer.insert(camera_buffer.end(), data_start, data.end());
    }
};

static std::mutex gui_mtx; /* Protects latest_frame */
static byteVector latest_frame;
static std::atomic_uint32_t latest_frame_id;
static std::atomic_bool save_next_frame = false;
static std::atomic_bool exit_program = false;
static constexpr std::string_view pic_dir = "pics";

static void pic_callback(const byteVector &pic)
{
    static uint32_t i = 0;

    std::cout << KCYN "PIC i:" << i << " size:" << pic.size() << KRST << std::endl;

    if (save_next_frame) {
        save_next_frame = false;
        std::ostringstream filename;
        auto tp = std::chrono::system_clock::now();

#ifdef __cpp_lib_format
        std::string date = std::format("{:%FT%T}", std::chrono::floor<std::chrono::seconds>(tp));
#else /* backup code for old compilers */
        std::time_t t = std::chrono::system_clock::to_time_t(tp);
        auto date = std::put_time(std::localtime(&t), "%FT%T");
#endif

        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count() % 1000;
        filename << pic_dir << "/frame_" << date
                 << "." << std::setfill('0') << std::setw(3) << millis << ".jpg";
        std::ofstream output(filename.str(), std::ios::binary);
        output.write(reinterpret_cast<const char *>(pic.data()), pic.size());
    }

    {
        std::lock_guard lock(gui_mtx);
        latest_frame = pic;
        latest_frame_id = i;
    }

    i++;
}

static void button_callback() {
    std::cout << KMAJ "BUTTON PRESS" KRST << std::endl;
    save_next_frame = true;
}

static void gui(void) {
    constexpr const char *window_name = "Geek szitman supercamera - PoC";
    uint32_t frame_done = latest_frame_id;

    while (!exit_program) {
        int key = cv::waitKey(10);
        if (key == 'q' or key == '\e') {
            exit_program = true;
        }

        if (frame_done != latest_frame_id) {
            cv::Mat img;
            {
                std::lock_guard lock(gui_mtx);
                img = cv::imdecode(latest_frame, cv::IMREAD_COLOR);
                frame_done = latest_frame_id;
            }
            if (img.data != nullptr) {
                cv::namedWindow(window_name, cv::WINDOW_AUTOSIZE);
                cv::imshow(window_name, img);
            }
        }
    }

    cv::destroyWindow(window_name);
}

static void upp(UsbSupercamera *usb_supercamera) {
    UPPCamera upp_camera(pic_callback, button_callback);
    byteVector read_buf;

    while (!exit_program) {
        int ret = usb_supercamera->read_frame(read_buf);
        if (ret == 0) {
            upp_camera.handle_upp_frame(read_buf);
        } else if (ret == LIBUSB_ERROR_NO_DEVICE) {
            exit_program = true;
        }
    }
}

int main(void)
{
    try {
        UsbSupercamera usb_supercamera;

        std::filesystem::create_directory(pic_dir);
        latest_frame_id = 0;

        std::thread upp_thread(upp, &usb_supercamera);
        gui();

        upp_thread.join();
        return 0;
    } catch (...) {
        return 1;
    }
}
