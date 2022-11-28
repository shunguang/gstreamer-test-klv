#ifndef __HOST_YUV_FRM_H__
#define __HOST_YUV_FRM_H__

#include "DataTypes.h"
namespace app {
	class HostYuvFrm {
	public:
		HostYuvFrm(const int w=0, const int h=0, const uint64_t fn=0 );  //sz = w*h*3/2
		HostYuvFrm(const int w, const int h, const uint64_t fn, const std::string &imgFilePath );
		HostYuvFrm(const cv::Mat &bgr, const uint64_t fn=0, const uint64_t ts=0);
		//soft copy, assert(bufSz == w*h*3/2)
		HostYuvFrm(const int w, const int h, uint8_t *buf_, uint32_t bufSz, const uint64_t fn, const uint64_t ts=0 );

		//copy construtor
		HostYuvFrm(const HostYuvFrm &x);
		~HostYuvFrm();

		HostYuvFrm& operator = (const HostYuvFrm &x);

		void resetSz(const int w, const int h);
		void setToZeros() {
			memset(buf_, 0, sz_);
		}

		void setToRand();

		void hdCopyTo(HostYuvFrm *dst) const;			//same size copy
		void hdCopyToLargerDst(HostYuvFrm *dst) const;	//dst.sz_ > src.sz_ copy
		void hdCopyToBgr(cv::Mat &picBGR);
		void hdCopyTo(uint8_t *buf, const uint32_t bufSz) const;
		
		uint32_t hdCopyFrom(const uint8_t *buf, const uint32_t bufSz, const uint64_t fn = 0, const uint64_t ts = 0);
		void hdCopyFromBgr(const cv::Mat &picBGR, const uint64_t fn=0, const uint64_t ts = 0);
		void wrtFrmNumOnImg();
		void drawRandomRoiAndwrtFrmNumOnImg( int nRois=10 );

		void dump(const std::string &folder, const std::string &tag = "", int roiW = 0, int roiH = 0, int L = 0);
	private:
		void creatBuf();
		void deleteBuf();

	public:
		uint64_t	fn_;		//frm # in original video for debug purpose
		uint64_t	ts_;		//ts in millisecond since epoch
		int			w_;			//width and height if its a video frm
		int			h_;
		uint32_t	sz_;		//buf size, assert( sz_ == w_*h_*3/2 );
		bool		isKeyFrm_;
		bool		isAllocatedBuf_;	//if only construct a head, we do not allocated buf on it
		uint8_t*	buf_;		//it's the yuv420p format
								//where YUV420p is a planar format, meaning that the Y, U, and V values are grouped together instead of interspersed.
								//The reason for this is that by grouping the U and V values together, the image becomes much more compressible.
								//When given an array of an image in the YUV420p format, all the Y? values come first, followed by all the U values, 
								//followed finally by all the V values.
								//to see the image: call yuv420p_save(const uint8_t *yuv420pRaw, const int sz, const int w, const int h, const char *fname);
		uint8_t	*pBuf_[3];      //pBuf_[0] = buf_; pBuf_[1] = buf_ + w_*h_; pBuf[2] = buf_ + w_*h_ * 5 / 4;
		uint32_t vSz_[3];		//vSz_[0] = w_*h_; vSz_[1] = w_*h_/4; vSz_[2] = w_*h_/4

	};
	typedef std::shared_ptr<HostYuvFrm>		HostYuvFrmPtr;
}
#endif
