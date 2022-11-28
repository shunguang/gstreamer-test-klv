#include "HostYuvFrm.h"

using namespace std;
using namespace app;

HostYuvFrm::HostYuvFrm( const int w, const int h, const uint64_t fn )
	: fn_(fn)
	, ts_(0)
	, w_(w)
	, h_(h)
	, sz_( 3*(w*h/2) )
	, isKeyFrm_(false)
	, isAllocatedBuf_(true)
	, buf_(NULL)
{
	creatBuf();
}

HostYuvFrm::HostYuvFrm(const int w, const int h, const uint64_t fn, const std::string &imgFilePath )
: HostYuvFrm( w, h, fn )
{
	cv::Mat bgr0 = cv::imread(imgFilePath, cv::IMREAD_COLOR);
	cv::Mat bgr1;
	cv::resize(bgr0, bgr1, cv::Size(w,h));
	hdCopyFromBgr(bgr1, fn);
}

HostYuvFrm::HostYuvFrm(const cv::Mat &bgr, const uint64_t fn, const uint64_t ts)
	: fn_(fn)
	, ts_(ts)
	, w_(bgr.cols)
	, h_(bgr.rows)
	, isKeyFrm_(false)
	, isAllocatedBuf_(true)
	, buf_(NULL)
{
	sz_ = 3 * (w_*h_) / 2;
	creatBuf();
	hdCopyFromBgr(bgr, fn);
}


//soft copy 
HostYuvFrm::HostYuvFrm( const int w, const int h, uint8_t *buf, uint32_t bufSz, const uint64_t fn, const uint64_t ts )
	: fn_(fn)
	, ts_(ts)
	, w_(w)
	, h_(h)
	, sz_(bufSz)
	, buf_( buf)
	, isKeyFrm_(false)
	, isAllocatedBuf_(false)
{
#if DEBUG_
	appAssert( bufSz == (w*h*3)/2, "HostYuvFrm::HostYuvFrm(): size does not match!");
#endif
}

HostYuvFrm::HostYuvFrm(const HostYuvFrm &x)
	: fn_(x.fn_)
	, ts_(x.ts_)
	, w_(x.w_)
	, h_(x.h_)
	, sz_(x.sz_)
	, isKeyFrm_(x.isKeyFrm_)
	, isAllocatedBuf_(true)
	, buf_(NULL)
{
	creatBuf();
	memcpy(buf_, x.buf_, x.sz_);
}

HostYuvFrm::~HostYuvFrm()
{
	deleteBuf();
}

HostYuvFrm &HostYuvFrm::operator = (const HostYuvFrm &x) 
{
	if (&x != this) { //check for self assignment
		if (sz_ != x.sz_) {
			deleteBuf();
			sz_ = x.sz_;
			creatBuf();
		}

		fn_ = x.fn_;
		ts_ = x.ts_;
		w_ = x.w_;
		h_ = x.h_;
		isKeyFrm_ = x.isKeyFrm_;
		memcpy(buf_, x.buf_, x.sz_);
	}

	return *this; // enables x=y=z;
}

void HostYuvFrm::resetSz(const int w, const int h)
{
	if ( w != w_ || h != h_ ) {
		deleteBuf();
		w_ = w; 
		h_ = h;
		sz_ = 3 * (w*h / 2);
		creatBuf();
	}
}

void HostYuvFrm::creatBuf()
{
	if( sz_ > 0 ){
		isAllocatedBuf_ = true;
		buf_ = new uint8_t[sz_];

		pBuf_[0] = buf_; 
		pBuf_[1] = buf_ + w_*h_; 
		pBuf_[2] = buf_ + w_*h_ * 5 / 4;

		vSz_[0] = w_*h_; 
		vSz_[1] = w_*h_/4; 
		vSz_[2] = vSz_[1];

		//for debug
		//assert(sz_ == vSz_[0] + vSz_[1] + vSz_[2]);

		if (!buf_) {
			printf("HostYuvFrm::creatBuf(): cannot allocate memory!");
			assert(0);
		}
	}
	else {
		buf_ = NULL;
		pBuf_[0] = NULL;
		pBuf_[1] = NULL;
		pBuf_[2] = NULL;

		vSz_[0] = 0;
		vSz_[1] = 0;
		vSz_[2] = 0;
	}
}

void HostYuvFrm::deleteBuf()
{
	if (isAllocatedBuf_) {
		delete[] buf_;
		buf_ = NULL;
	}
}


void HostYuvFrm::hdCopyToBgr(cv::Mat &picBGR) 
{
	cv::Mat picYV12 = cv::Mat(h_ * 3 / 2, w_, CV_8UC1, buf_);
	cv::cvtColor(picYV12, picBGR, CV_YUV420p2RGB);
}


void HostYuvFrm::wrtFrmNumOnImg()
{
	cv::Mat bgr;

	uint64_t dt = TS_SINCE_EPOCH_MILLISEC - ts_;
	cv::Mat picYV12 = cv::Mat(h_ * 3 / 2, w_, CV_8UC1, buf_);
	cv::cvtColor(picYV12, bgr, CV_YUV420p2RGB);

	cv::putText( bgr, (std::to_string(fn_) + /*", dt=" + std::to_string(dt) + */ ", [" + std::to_string(w_) + "x" + std::to_string(h_) + "]"), 
		cv::Point(5, h_-10), cv::FONT_HERSHEY_COMPLEX_SMALL, 2, cv::Scalar(255, 255, 255), 2);
	cv::cvtColor(bgr, picYV12, cv::COLOR_BGR2YUV_I420);
}

void HostYuvFrm::setToRand()
{
	for (uint32_t i=0; i<sz_; ++i){
		buf_[i] = rand()%255;
	}
}

void HostYuvFrm::drawRandomRoiAndwrtFrmNumOnImg(int nRois)
{
	cv::Mat bgr;
	int x0, y0, w0, h0;
	cv::Mat picYV12 = cv::Mat(h_ * 3 / 2, w_, CV_8UC1, buf_);
	cv::cvtColor(picYV12, bgr, CV_YUV420p2RGB);
	for (int i = 0; i < nRois; ++i) {
		x0 = rand() % w_;
		y0 = rand() % h_;
		w0 = rand() % (w_ - x0);
		h0 = rand() % (h_ - y0);
		
		cv::Rect r0(x0, y0, w0, h0);
		cv::rectangle(bgr, r0, cv::Scalar(rand() % 255, rand() % 255, rand() % 255));
	}

	cv::putText(bgr, std::to_string(fn_), cv::Point(5, h_-100), cv::FONT_HERSHEY_COMPLEX_SMALL, 2.5, cv::Scalar(255, 255, 0), 2);
	cv::cvtColor(bgr, picYV12, cv::COLOR_BGR2YUV_I420);
}


void HostYuvFrm::hdCopyTo(HostYuvFrm *dst) const
{
	*dst = *this;
}

void HostYuvFrm::hdCopyTo(uint8_t *buf, const uint32_t bufSz) const
{
	if (bufSz != sz_) {
		printf("HostYuvFrm::hdCopyTo(): error buf sz doest match, data loss!\n");
		assert(0);
	}
	memcpy(buf, buf_, bufSz);
}

void HostYuvFrm::hdCopyToLargerDst(HostYuvFrm *dst) const
{
	int wSrc = w_, hSrc = h_;
	int wDst = dst->w_;

	dst->fn_ = fn_;
	dst->ts_ = ts_;
	
	for (int ch = 0; ch < 3; ++ch) {
		if (ch > 0) {
			wSrc = w_ / 2;
			hSrc = h_ / 2;
			wDst = dst->w_ / 2;
		}

		uint8_t *pDst = dst->pBuf_[ch];
		uint8_t *pSrc = pBuf_[ch];
		for (int y = 0; y < hSrc; ++y, pDst += wDst, pSrc += wSrc) {
			memcpy(pDst, pSrc, wSrc);
		}
	}
}

uint32_t HostYuvFrm::hdCopyFrom(const uint8_t *buf, const uint32_t bufSz, const uint64_t fn, const uint64_t ts) 
{
	if (bufSz != sz_) {
		printf("HostYuvFrm::copyFrom(): buf sz doest match, data loss!\n");
		return 0;
	}
	memcpy(buf_, buf, bufSz);
	fn_ = fn;
	ts_ = ts;
	return bufSz;
}

void HostYuvFrm::hdCopyFromBgr(const cv::Mat &bgr, const uint64_t fn, const uint64_t ts)
{
	assert(w_ == bgr.cols && h_ == bgr.rows);

#if 1
	//not tested yet
	cv::Mat yuv420 = cv::Mat(h_ * 3 / 2, w_, CV_8UC1, buf_);
	cv::cvtColor(bgr, yuv420, cv::COLOR_BGR2YUV_I420);
#else
	//old workable, but too expensive
	cv::Mat yuv420;
	cv::cvtColor(bgr, yuv420, cv::COLOR_BGR2YUV_I420);
	hdCopyFrom(yuv420.data, sz_, fn);
#endif
	fn_ = fn;
	ts_ = ts;
}


void HostYuvFrm::dump(const std::string &folder, const std::string &tag, int roiW, int roiH, int L)
{
	char buf[1024];
	snprintf(buf, 1024, "%s/%s-fn-%06llu_L%d.png", folder.c_str(), tag.c_str(), fn_, L);
	cv::Mat bgr;
	hdCopyToBgr(bgr);
	if (roiW == 0 || roiH == 0) {
		cv::imwrite(buf, bgr);
	}
	else {
		cv::Rect roi(0, 0, roiW, roiH);
		cv::Mat I = bgr(roi);
		cv::imwrite(buf, I);
	}
}
