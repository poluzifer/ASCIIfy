#include <opencv2/opencv.hpp>
#include <iostream>
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <fstream>
#include <shobjidl.h>

std::vector<std::string> SaveFile;

std::string OpenFileDialog() {
    OPENFILENAMEA ofn;       
    CHAR szFile[260] = { 0 }; 
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;  
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Image Files\0*.jpg;*.jpeg;*.png;*.bmp\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameA(&ofn)) {
        return std::string(ofn.lpstrFile);
    }
    else {
        return "";
    }
}

bool SaveTextFileDialog(const std::vector<std::string>& lines) {
    HRESULT hr;
    IFileSaveDialog* pSaveDlg = nullptr;

    hr = CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pSaveDlg));
    if (FAILED(hr)) return false;

    COMDLG_FILTERSPEC rgSpec[] = {
        {L"Text Files", L"*.txt"}
    };
    pSaveDlg->SetFileTypes(1, rgSpec);
    pSaveDlg->SetDefaultExtension(L"txt");

    hr = pSaveDlg->Show(nullptr);
    if (SUCCEEDED(hr)) {
        IShellItem* pItem;
        hr = pSaveDlg->GetResult(&pItem);
        if (SUCCEEDED(hr)) {
            PWSTR pszFilePath = nullptr;
            pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);

            std::ofstream ofs(pszFilePath);
            if (ofs.is_open()) {
                for (const std::string& line : lines)
                    ofs << line << "\n";
                ofs.close();
                std::wcout << L"Saved to: " << pszFilePath << std::endl;
            }

            CoTaskMemFree(pszFilePath);
            pItem->Release();
        }
    }
    pSaveDlg->Release();
    return SUCCEEDED(hr);
}

std::string mapAngleToASCII(double angle) {
    if ((angle >= 337.5 || angle < 22.5))  return "|";   
    if (angle >= 22.5 && angle < 67.5)    return "/";   
    if (angle >= 67.5 && angle < 112.5)   return "-";   
    if (angle >= 112.5 && angle < 157.5)   return "\\";  
    if (angle >= 157.5 && angle < 202.5)   return "|";  
    if (angle >= 202.5 && angle < 247.5)   return "/";   
    if (angle >= 247.5 && angle < 292.5)   return "-";   
    if (angle >= 292.5 && angle < 337.5)   return "\\"; 
    return "."; 
}

char mapBrightnessToASCII(int brightness) {
    const std::string levels = "@%#*+=-:. ";
    size_t len = levels.size();
    int index = brightness * (static_cast<int>(len - 1)) / 255;
    return levels[index];
}

void ImageToASCII(const cv::Mat& image, int width_size) {
    if (image.empty()) {
        std::cout << "Image not found!" << std::endl;
        return;
    }

    cv::Mat gray;
    cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);

    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE();
    clahe->apply(gray, gray);

    int height_size = static_cast<int>(gray.rows * 0.5 * width_size / gray.cols);

    cv::resize(gray, gray, cv::Size(width_size, height_size), 0, 0, cv::INTER_AREA);

    SaveFile.clear();

    for (int y = 0; y < gray.rows; ++y) {
        std::string line;
        for (int x = 0; x < gray.cols; ++x) {
            int brightness = gray.at<uchar>(y, x);
            std::cout << mapBrightnessToASCII(brightness);
            line.push_back(mapBrightnessToASCII(brightness));
        }
        std::cout << "\n";
        SaveFile.push_back(line);
    }
}

void ImageToContours(const cv::Mat& image, int width_size) {
    if (image.empty()) {
        std::cout << "Image not found!" << std::endl;
        return;
    }

    cv::Mat gray;
    cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);

    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE();
    clahe->apply(gray, gray);

    int height_size = static_cast<int>(gray.rows * 0.5 * width_size / gray.cols);
    cv::resize(gray, gray, cv::Size(width_size, height_size), 0, 0, cv::INTER_AREA);

    double medianVal = cv::mean(gray)[0];
    double lower = std::max(0.0, 0.66 * medianVal);
    double upper = std::min(255.0, 1.33 * medianVal);
    cv::Canny(gray, gray, lower, upper);

    cv::Mat grad_x, grad_y;
    cv::Sobel(gray, grad_x, CV_64F, 1, 0, 3);
    cv::Sobel(gray, grad_y, CV_64F, 0, 1, 3);

    cv::Mat magnitude, angle;
    cv::cartToPolar(grad_x, grad_y, magnitude, angle, true);

    SaveFile.clear();

    for (int y = 0; y < gray.rows; ++y) {
        std::string line;
        for (int x = 0; x < gray.cols; ++x) {
            if (gray.at<uchar>(y, x) == 255) {
                double theta = angle.at<double>(y, x);
                std::cout << mapAngleToASCII(theta);
                line += mapAngleToASCII(theta);
            }
            else {
                std::cout << " ";
                line += ' ';
            }
        }
        std::cout << "\n";
        SaveFile.push_back(line);
    }
}
int main() {
    std::string path = OpenFileDialog();
    if (path.empty()) {
        std::cout << "No file selected!" << std::endl;
        return 1;
    }

    cv::Mat image = cv::imread(path);
    if (image.empty()) {
        std::cout << "Failed to load image!" << std::endl;
        return 1;
    }

    std::cout << "Choose mode:\n" << "1 - ASCII image\n" << "2 - Contours image\n" << "Enter mode: ";

    int choice = 0;

    std::cin >> choice;

    std::cout << "Choose size:\n" << "The entered size will be the width of the image in pixels" << std::endl;
    int size = 0;
    std::cin >> size;

    if (choice == 1) {
        ImageToASCII(image, size);
    }
    else if (choice == 2) {
        ImageToContours(image, size);
    }
    else {
        std::cout << "Invalid choice!" << std::endl;
        return 1;
    }

    std::cout << "\n Do you want save image to file?  (Y/N)" << std::endl;
    char check;
    std::cin >> check;
    check = std::toupper(check);
    if (check == 'Y') {
        SaveTextFileDialog(SaveFile);
    }
    else if (check == 'N') {
        return 0;
    }
    else {
        std::cout << "Invalid choice!" << std::endl;
        return 1;
    }

    return 0;
}
