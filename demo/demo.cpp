#include "htwk_vision.h"

#include <chrono>

#include <boost/program_options.hpp>

#include <htwkpngimageprovider.h>
#include <htwkyuv422image.h>
#include <hypotheses_generator_scanlines.h>

//#include <valgrind/callgrind.h>

using namespace htwk;
using namespace htwk::image;
using namespace std::chrono;
using namespace std;
namespace bpo = boost::program_options;
namespace bf  = boost::filesystem;

#define STD_WIDTH  640
#define STD_HEIGHT 480

inline void saveAsPng(uint8_t* img, int width, int height, const std::string& filename, Yuv422Image::Filter filter = Yuv422Image::NONE) {
    static PngImageSaverPtr pngSaver = getPngImageSaverInstace();
    Yuv422Image yuvImage(img, width, height);
    yuvImage.saveAsPng(pngSaver, filename, filter);
}

inline void setY(uint8_t* const img, const uint32_t width, const int32_t x, int32_t y, const uint8_t c) {
    img[(x+y*width)<<1]=c;
}

void drawFieldColor(const string &name, const uint8_t *const orig_img, FieldColorDetector *fcd, int width, int height) {
    uint8_t *img = (uint8_t*) malloc(sizeof(uint8_t) * width * height * 2);
    memcpy(img, orig_img, sizeof(uint8_t) * width * height * 2);
    for(int y=0;y<height;y++){
        for(int x=0;x<width;x++){
            int cy=fcd->getY(img,x,y);
            int cb=fcd->getCb(img,x,y);
            int cr=fcd->getCr(img,x,y);
            if(fcd->isGreen(cy,cb,cr)){
                setY(img,width,x,y,0);
            }
        }
    }

    saveAsPng(img, width, height, name+"_fieldcolordetector.png");
    free(img);
}

void drawLineSegments(const string &name, const uint8_t * const orig_img, RegionClassifier *rc, int width, int height){
    std::vector<LineSegment*> * lineSegments = rc->lineSegments;
    uint8_t *img = (uint8_t*) malloc(sizeof(uint8_t) * width * height * 2);
    memcpy(img, orig_img, sizeof(uint8_t) * width * height * 2);
    for (const LineSegment *ls : *lineSegments){
        if(ls->x<0||ls->x>=width||ls->y<0||ls->y>=height)continue;
        for(float d=0;d<=5;d+=0.1){
            int px=(int)(ls->x+ls->vx*d);
            int py=(int)(ls->y+ls->vy*d);
            if(px<0||py<0||px>=width||py>=height)continue;
            setY(img,width,px,py,255);
        }
        setY(img,width,ls->x,ls->y,0);
    }

    saveAsPng(img, width, height, name + "_regionclassifier.png");
}

void drawScanLines(const string &name, const uint8_t * const orig_img, RegionClassifier *rc, int width, int height){
    uint8_t *img = (uint8_t*) malloc(sizeof(uint8_t) * width * height * 2);
    memcpy(img, orig_img, sizeof(uint8_t) * width * height * 2);

    Scanline* scanVertical=rc->getScanVertical();
    for(int k=0;k<rc->getScanVerticalSize();k++){
        Scanline sl=scanVertical[k];
        for (int i=sl.edgeCnt;i>0;i--){
//            std::cout << sl.edgesY[i-1] << " -> " << sl.edgesY[i] << std::endl;
            for (int y=sl.edgesY[i];y<sl.edgesY[i-1];y++){
//                setY(img,width,sl.edgesX[0],y,0);
                if (sl.regionsIsGreen[i-1]){
                    setY(img,width,sl.edgesX[0],y,255);
                }
                if (sl.regionsIsWhite[i-1]){
                    setY(img,width,sl.edgesX[0],y,0);
                }
            }
        }
    }

    Scanline* scanHorizontal=rc->getScanHorizontal();
    for(int k=0;k<rc->getScanHorizontalSize();k++){
        Scanline sl=scanHorizontal[k];
        for (int i=sl.edgeCnt;i>0;i--){
//            std::cout << sl.edgesY[i-1] << " -> " << sl.edgesY[i] << std::endl;
            for (int x=sl.edgesX[i];x<sl.edgesX[i-1];x++){
//                setY(img,width,sl.edgesX[0],y,0);
                if (sl.regionsIsGreen[i-1]){
                    setY(img,width,x,sl.edgesY[0],255);
                }
                if (sl.regionsIsWhite[i-1]){
                    setY(img,width,x,sl.edgesY[0],0);
                }
            }
        }
    }

    saveAsPng(img, width, height, name+"_scanlines.png");
    free(img);
}

void drawFieldBorder(const string &name, const uint8_t * const orig_img, FieldDetector *fd, RegionClassifier *rc, int width, int height) {
    uint8_t *img = (uint8_t*) malloc(sizeof(uint8_t) * width * height * 2);
    memcpy(img, orig_img, sizeof(uint8_t) * width * height * 2);
    const int *fieldBorder = fd->getConvexFieldBorder();
    for(int x=0;x<width;x++){
        int y=fieldBorder[x];
        if(y<0||y>=height)continue;
        setY(img,width,x,y,255);
    }

    saveAsPng(img, width, height, name+"_fielddetector.png");
    free(img);
}

void drawLineEdge(uint8_t *img, const LineEdge &ls, int c, LineDetector *ld, int width, int height){
    for(float d=0;d<=1;d+=0.001){
        int px=(int)round(ls.px1*(1-d)+ls.px2*d);
        int py=(int)round(ls.py1*(1-d)+ls.py2*d);
        if(px<0||py<0||px>=width||py>=height)continue;
        setY(img,width,px,py,c);
    }
}


void drawRobots(const string &name, const uint8_t * const orig_img, std::vector<RobotClassifierResult> robots, int width, int height){
    uint8_t *img = (uint8_t*) malloc(sizeof(uint8_t) * width * height * 2);
    memcpy(img, orig_img, sizeof(uint8_t) * width * height * 2);
    for(const RobotClassifierResult &it:robots){
        int p=(int)(255*it.detectionProbability);
        for(int px=it.rect.xLeft;px<=it.rect.xRight;px++){
            setY(img,width,px,it.rect.yTop,p);
            setY(img,width,px,it.rect.yBottom,p);
        }
        for(int py=it.rect.yTop;py<=it.rect.yBottom;py++){
            setY(img,width,it.rect.xLeft,py,p);
            setY(img,width,it.rect.xRight,py,p);
        }
    }

    saveAsPng(img, width, height, name+"_robotdetector.png");
    free(img);
}

void drawLines(const string &name, const uint8_t * const orig_img, LineDetector *ld, int width, int height){
    uint8_t *img = (uint8_t*) malloc(sizeof(uint8_t) * width * height * 2);
    memcpy(img, orig_img, sizeof(uint8_t) * width * height * 2);
    std::vector<LineGroup> linesList=ld->getLineGroups();
    for(const LineGroup &it:linesList){
        drawLineEdge(img,it.lines[0],0, ld, width, height);
        drawLineEdge(img,it.lines[1],0, ld, width, height);
    }

    saveAsPng(img, width, height, name+"_linedetector.png");
    free(img);
}

void drawGoalPosts(const string &name, const uint8_t * const orig_img, GoalDetector *gd, int width, int height){
    uint8_t *img = (uint8_t*) malloc(sizeof(uint8_t) * width * height * 2);
    memcpy(img, orig_img, sizeof(uint8_t) * width * height * 2);
    for(const GoalPost &gp : gd->getGoalPosts()){
        //std::cout << "GoalPost: " << gp.probability << std::endl;
        int p=(int)(255*gp.probability);

        for(int ny=gp.upperPoint.y;ny<=gp.basePoint.y;ny++){
            double f=(double)(ny-gp.upperPoint.y)/(gp.basePoint.y-gp.upperPoint.y);
            int nx=(int)(gp.upperPoint.x*(1-f)+gp.basePoint.x*f);
            if(nx<0||nx>=width)continue;
            setY(img, width, nx, ny, p);
        }

        for(int dx=-8;dx<=8;dx++){
            int px=gp.basePoint.x+dx;
            if(px<0||px>=width||gp.basePoint.y<0||gp.basePoint.y>=height)break;
            setY(img,width,px,(int)gp.basePoint.y,p);
        }
    }

    saveAsPng(img, width, height, name+"_goaldetector.png");
    free(img);
}

void drawHypothesis(const string &name, const uint8_t * const orig_img, HypothesesGenerator *hg, int width, int height){
//void drawHypothesis(const string &name, const uint8_t * const orig_img, RobotAreaDetector *hg, int width, int height){
    uint8_t *img = (uint8_t*) malloc(sizeof(uint8_t) * width * height * 2);
    memcpy(img, orig_img, sizeof(uint8_t) * width * height * 2);
    for (const ObjectHypothesis &it:hg->getHypotheses()){
        int startX=it.x-it.r, endX=it.x+it.r;
        int startY=it.y-it.r, endY=it.y+it.r;
        int p=(int)(255*it.rating);
        if (startX<0 || startY<0 || width<endX || height<endY)
            continue;

        for (int i=startX;i<endX;i++){
            setY(img,width,i,startY,p);
            setY(img,width,i,endY,p);
        }
        for (int i=startY;i<endY;i++){
            setY(img,width,startX,i,p);
            setY(img,width,endX,i,p);
        }
    }

    saveAsPng(img, width, height, name+"_hypothesisGen.png");
    free(img);
}

void drawALLHypothesis(const string &name, const uint8_t * const orig_img, HypothesesGenerator *hg, int width, int height){
    uint8_t *img = (uint8_t*) malloc(sizeof(uint8_t) * width * height * 2);
    int cnt=0;
    for (const ObjectHypothesis &it:hg->getHypotheses()){
        memcpy(img, orig_img, sizeof(uint8_t) * width * height * 2);
        int startX=it.x-it.r, endX=it.x+it.r;
        int startY=it.y-it.r, endY=it.y+it.r;
        //int p=std::min(255,std::max(0,(int)(it.rating/2.4f))); // 5Points
        //int p=std::min(255,std::max(0,(int)(it.rating/3.1))); // Integral
        //int p=std::min(255,std::max(0,(int)(it.rating/1.5))); // inner-outer
        //int p=std::min(255,std::max(0,(int)(it.rating/7.65))); // cross
        int p=std::min(255,std::max(0,(int)(it.rating/2.4f))); // crossPoints
        if (startX<0 || startY<0 || width<endX || height<endY)
            continue;

        for (int i=startX;i<endX;i++){
            setY(img,width,i,startY,p);
            setY(img,width,i,endY,p);
        }
        for (int i=startY;i<endY;i++){
            setY(img,width,startX,i,p);
            setY(img,width,endX,i,p);
        }
        saveAsPng(img, width, height, name+"_hypo_"+std::to_string(cnt)+"_"+std::to_string(p)+".png");
        cnt++;
    }

    free(img);
}

void drawRatingHypo(const string &name, const uint8_t * const orig_img, HypothesesGenerator *hg, int width, int height){
    uint8_t *img = (uint8_t*) malloc(sizeof(uint8_t) * width * height * 2);
    memcpy(img, orig_img, sizeof(uint8_t) * width * height * 2);

      //Hypo Blocks
//    int iWidth = (int)width*0.5;
//    int iHeight = (int)height*0.5;
//    for (int y=0;y<iHeight;y++){
//        for (int x=0;x<iWidth;x++){
//            int cy=std::max(0,std::min((int)(128+0.1*hg->getRatingImg()[x+y*iWidth]),255));
//            setY(img,width,x*2,y*2,cy);
//            setY(img,width,x*2+1,y*2,cy);
//            setY(img,width,x*2,y*2+1,cy);
//            setY(img,width,x*2+1,y*2+1,cy);
//        }
//    }

//--------Hypo Blur
    int scale = 4;
    int iWidth = (int)width/scale;
    int iHeight = (int)height/scale;
    for (int y=0;y<iHeight;y++){
        for (int x=0;x<iWidth;x++){
            int cy=std::max(0,std::min((int)(0.1*hg->getRatingImg()[x+y*iWidth]),255));
            for (int ny=y*scale;ny<y*scale+scale;ny++){
                for (int nx=x*scale;nx<x*scale+scale;nx++){
                    setY(img,width,nx,ny,cy);
                }
            }
        }
    }
////--------ScanLines
    HypothesesGeneratorScanlines* hgv = (HypothesesGeneratorScanlines*)hg;
    for (int i=0;i<hgv->svMax;i++){
        for (int j=0;j<hgv->getScanVertical()[i].edgeCnt;j++){
            if (hgv->getScanVertical()[i].regionsIsObj[j]){
                int x=hgv->getScanVertical()[i].edgesX[j];
                int y=hgv->getScanVertical()[i].edgesY[j];
                if (j>0) y=hgv->getScanVertical()[i].edgesY[j-1];
                for (;y<=hgv->getScanVertical()[i].edgesY[j];y++){
                    for(int ny=y*scale;ny<y*scale+scale;ny++){
                        setY(img,width,x*scale,ny,0);
                    }
                }
            }
        }
    }

    for (int i=0;i<hgv->shMax;i++){
        for (int j=0;j<hgv->getScanHorizontal()[i].edgeCnt;j++){
            if (hgv->getScanHorizontal()[i].regionsIsObj[j]){
                int y=hgv->getScanHorizontal()[i].edgesY[j];
                int x=hgv->getScanHorizontal()[i].edgesX[j];
                if (j>0) x=hgv->getScanHorizontal()[i].edgesX[j-1];
                for (;x<=hgv->getScanHorizontal()[i].edgesX[j];x++){
                    for(int nx=x*scale;nx<x*scale+scale;nx++){
                        setY(img,width,nx,y*scale,0);
                    }
                }
            }
        }
    }

    std::string newName = name+"_ratingIMG.png";
    printf("XXX %s\n", newName.c_str());
    saveAsPng(img, width, height, name+"_ratingIMG.png", Yuv422Image::CONVERT_TO_GREY);
    free(img);
}

void drawBall(const string &name, const uint8_t * const orig_img, BallDetector *bd, int width, int height){
    uint8_t *img = (uint8_t*) malloc(sizeof(uint8_t) * width * height * 2);
    memcpy(img, orig_img, sizeof(uint8_t) * width * height * 2);
    if(bd->isBallFound()){
        int bx=bd->getBall().x;
        int by=bd->getBall().y;
        int r=bd->getBall().r;
        for(float a=0;a<M_PI*2;a+=0.01){
            int nx=bx+sin(a)*r;
            int ny=by+cos(a)*r;
            if(nx<0||ny<0||nx>=width||ny>=height)continue;
            setY(img,width,nx,ny,255);
        }
    }

    saveAsPng(img, width, height, name+"_balldetector.png");
    free(img);
}

void drawCircle(const string &name, const uint8_t * const orig_img, RansacEllipseFitter *raf, RegionClassifier *rc, int width, int height){
    uint8_t *img = (uint8_t*) malloc(sizeof(uint8_t) * width * height * 2);
    memcpy(img, orig_img, sizeof(uint8_t) * width * height * 2);
    if(raf->getEllipse().found){
        float stepWidth=M_PI*2/64;
        float lastX=0;
        float lastY=0;
        for(double a=0;a<=M_PI*2+0.00001;a+=stepWidth){
            float x=sinf(a);
            float y=cosf(a);
            point_2d point;
            point.x=x*raf->getEllipse().ta;
            point.y=y*raf->getEllipse().tb;
            raf->transformPoInv(point, raf->getEllipse().trans, raf->getEllipse().translation);
            float px=point.x;
            float py=point.y;
            if(a>0){
                for(float d=0;d<=1;d+=0.01){
                    int nx=(int)round(px*(1-d)+lastX*d);
                    int ny=(int)round(py*(1-d)+lastY*d);
                    if(nx<0||ny<0||nx>=width||ny>=height)continue;
                    setY(img,width,nx,ny,0);
                }
            }
            lastX=px;
            lastY=py;
        }
    }

    saveAsPng(img, width, height, name+"_ellipsefitter.png");
    free(img);
}

void drawFeet(const string &name, const uint8_t * const orig_img, FeetDetector *fd, int width, int height){
    uint8_t *img = (uint8_t*) malloc(sizeof(uint8_t) * width * height * 2);
    memcpy(img, orig_img, sizeof(uint8_t) * width * height * 2);
    int px=fd->getBase().x;
    if(px>=0&&px<width){
        for(int y=0;y<height;y++){
            setY(img,width,px,y,255);
        }
    }
    int py=fd->getBase().y;
    if(py>=0&&py<height){
        for(int x=0;x<width;x++){
            setY(img,width,x,py,255);
        }
    }

    saveAsPng(img, width, height, name+"_feetdetector.png");
    free(img);
}

void handleProgrammArguments(bpo::variables_map& varmap, int argc, char** argv) {
    bpo::options_description options_all("");
    options_all.add_options()
            ("help,h",                                              "Print this help")
            ("dir,d",          bpo::value<std::string>(),           "Process a directory of images")
            ("image,i",        bpo::value<std::string>(),           "The image that should be processed")
            ("cycles,c",       bpo::value<int>()->default_value(10), "How often process a image with the vision.")
            ("resultImages,r", bpo::value<std::string>()->default_value(""), "Path for result images.")
            ("writeTimeFile,w",    bpo::value<bool>()->default_value(false), "Write a time.csv file");

    bpo::store(bpo::parse_command_line(argc, argv, options_all), varmap); //parse and store

    bpo::notify(varmap); // update the varmap

    if(varmap.count("help")) { std::cout << options_all; exit(0); }
    if(varmap.count("image") == 0 && varmap.count("dir") == 0) { std::cout << options_all; exit(0); }
}

uint8_t* loadFile(const std::string& filename, float& pitch, float& roll) {
    uint8_t *imageYUV422 = nullptr;
    size_t bufferSize = sizeof(uint8_t) * 2 * STD_WIDTH * STD_WIDTH;

    if(posix_memalign((void**)&imageYUV422, 16, bufferSize) != 0) {
        std::cout << "error allocating aligned memory! reason: " << strerror(errno) << std::endl;
        exit(1);
    }

    if(imageYUV422 == nullptr) {
        std::cout << "Couldn't allocate yuv memory. Exit now." << std::endl;
        exit(1);
    }

    PngImageProviderPtr pngImageProvider = getPngImageProviderInstace(STD_WIDTH, STD_HEIGHT);
    pngImageProvider->loadAsYuv422(filename, imageYUV422, bufferSize, pitch, roll);

    return imageYUV422;
}

void writeDebugFiles(const string &name, uint32_t width, uint32_t height, uint8_t* imageYUV422, HTWKVision &vision)
{
//    drawFieldColor(name, imageYUV422, vision.fieldColorDetector, width, height);
//    drawLineSegments(name, imageYUV422, vision.regionClassifier, width, height);
//    drawScanLines(name, imageYUV422, vision.regionClassifier, width, height);
    drawFieldBorder(name, imageYUV422, vision.fieldDetector, vision.regionClassifier, width, height);
//    drawLines(name, imageYUV422, vision.lineDetector, width, height);
//    drawGoalPosts(name, imageYUV422, vision.goalDetector, width, height);
    drawRatingHypo(name,imageYUV422,vision.hypothesesGenerator, width, height);
//    drawALLHypothesis(name,imageYUV422,vision.hypothesesGenerator, width, height);
//    drawHypothesis(name,imageYUV422,vision.hypothesesGenerator, width, height);
//    drawHypothesis(name,imageYUV422,vision.robotAreaDetector, width, height);
//    drawBall(name, imageYUV422, vision.ballDetector, width, height);
//    drawCircle(name, imageYUV422, vision.ellipseFitter, vision.regionClassifier, width, height);
//    drawFeet(name, imageYUV422, vision.feetDetector, width, height);
//    drawRobots(name, imageYUV422, vision.getRobotClassifierResult(), width, height);
}

void processOneFile(const std::string& filename, int processingCount, const std::string &debugPath) {
    float pitch=0;
    float roll=0;
    uint8_t* imageYUV422 = loadFile(filename, pitch, roll);

    high_resolution_clock::time_point t1 = high_resolution_clock::now();

    HTWKVision vision(STD_WIDTH, STD_HEIGHT, HtwkVisionConfig());
    vision.resetProfilingStats(true);
    for (int i=0;i<processingCount;i++) {
        vision.proceed(imageYUV422, true, pitch, roll);
    }
    vision.printProfilingResults(true);

    high_resolution_clock::time_point t2 = high_resolution_clock::now();
    duration<double> time_span = duration_cast<duration<double>>(t2 - t1);
    std::cout << "time: " << time_span.count() * 1000 << "ms\n";

    if (!debugPath.empty()) {
        int start = filename.rfind('/')+1;
        writeDebugFiles(debugPath+"/"+filename.substr(start,filename.size()-start-4), STD_WIDTH, STD_HEIGHT, imageYUV422, vision);
    }

    free(imageYUV422);
}

struct ipr {
    uint8_t *img;
    float pitch;
    float roll;
    std::string name;
};

void processDirectory(const std::string& path, int processingCount, const std::string &debugPath, const bool writeTimeFile) {
    std::deque<ipr> images;

    /* We don't want to see all the loading and converting of pngs. We cache the raw data in memory */
    const bf::recursive_directory_iterator end;

    bf::path startPath(path);
    for(auto it = bf::recursive_directory_iterator(startPath); it != end; ++it) {
        if(bf::is_regular_file(*it) && it->path().extension() == ".png") {
            std::cout << "Load " << it->path() << std::endl;
            std::string filename = it->path().string();
            ipr img;
            img.pitch=0;
            img.roll=0;
            img.name=filename;
            img.img = loadFile(filename, img.pitch, img.roll);

            images.push_back(img);
        }
    }

    std::cout << std::endl << std::endl << "Processing " << images.size() << " images." << std::endl;
    high_resolution_clock::time_point t1 = high_resolution_clock::now();
//    ProfilerStart("vision.prof");

//    CALLGRIND_START_INSTRUMENTATION;
//    CALLGRIND_TOGGLE_COLLECT;

    HtwkVisionConfig upperConfig;
    HtwkVisionConfig lowerConfig;
    lowerConfig.isUpperCam = false;
    HTWKVision upperVision(STD_WIDTH, STD_HEIGHT, upperConfig);
    HTWKVision lowerVision(STD_WIDTH, STD_HEIGHT, lowerConfig);
    upperVision.resetProfilingStats(true);
    lowerVision.resetProfilingStats(true);
    std::string statsFileUpper;
    std::string statsFileLower;
    statsFileUpper.append(path + "/timeUpper.csv");
    statsFileLower.append(path + "/timeLower.csv");

    for (const ipr &img : images){
        boost::filesystem::path p(img.name);
        std::string imgName = p.filename().string();
        bool isUpper = (imgName.find("_U.") != std::string::npos);
        std::cout << imgName << std::endl;

        if(isUpper) {
            for (int i=0;i<processingCount;i++){
                upperVision.proceed(img.img, false,img.pitch,img.roll);
            }
            if (writeTimeFile)
                upperVision.writeProfilingFile(statsFileUpper,imgName);
        } else {
            for (int i=0;i<processingCount;i++){
                lowerVision.proceed(img.img,true,img.pitch,img.roll);
            }
            if (writeTimeFile)
                lowerVision.writeProfilingFile(statsFileLower,imgName);
        }

        if (!debugPath.empty()) {
            writeDebugFiles(debugPath+"/"+imgName, STD_WIDTH, STD_HEIGHT, img.img, upperVision);
        }
    }
    if(!writeTimeFile){
        printf("\n\nUpper Camera timing:\n");
        upperVision.printProfilingResults(true);
        printf("\nLower Camera timing:\n");
        lowerVision.printProfilingResults(true);
    }

//    CALLGRIND_TOGGLE_COLLECT;
//    CALLGRIND_STOP_INSTRUMENTATION;

//    ProfilerStop();
    high_resolution_clock::time_point t2 = high_resolution_clock::now();
    duration<double> time_span = duration_cast<duration<double>>(t2 - t1);
    std::cout << "time: " << time_span.count() * 1000 << "ms\n";

    for (const ipr &img : images) { free(img.img); }
}

int main(int argc, char **argv) {
    bpo::variables_map varmap;
    handleProgrammArguments(varmap, argc, argv);

    const int  processingCount  = max(1,varmap["cycles"].as<int>());
    const bool writeTime =varmap["writeTimeFile"].as<bool>();
    string writeDebugFiles =   varmap["resultImages"].as<string>();

    if(varmap.count("image")) {
        std::string filename = varmap["image"].as<std::string>();
        processOneFile(filename, processingCount, writeDebugFiles);
    } else {
        std::string path = varmap["dir"].as<std::string>();
        processDirectory(path, processingCount, writeDebugFiles, writeTime);
    }
}
