package com.sina.sinavideo.coreplayer.probe;

import java.io.Serializable;

public class VDProbeServiceData implements Serializable {

	/**
	 * 
	 */
	private static final long serialVersionUID = 7053516470255747051L;

	// 静态属性部分
	public String mAudioBitrate = ""; // 音频流比特率
	public String mAudioHZ = ""; // 音频流比特率
	public String mVideoWxH = ""; // 视频原始画面大小
	public String mAudioChannel = ""; // 音频流声道数
	public String mFormat = ""; // 文件格式
	public String mCodec = ""; // 编码格式
	public String mPixFmt = ""; // 视频编码器使用的图像格式
	public String mDuration = ""; // 时长

	// 动态属性部分
	public String mCPUInfo = "";
	public String mRamInfo = "";
	public String mFPSInfo = "";

	public VDProbeServiceData() {
		super();
	}
}
