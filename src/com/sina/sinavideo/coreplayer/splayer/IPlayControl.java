package com.sina.sinavideo.coreplayer.splayer;

public interface IPlayControl {
	public void setPlayURL(String path);
	public void start();
	public void pause();
	public void release();
	public boolean isPlaying();
	public int getCurrentPosition();
	public int getDuration();
	public void seekTo(int msec);
	public void config(String name, int val);
	public void setVolume(float leftVolume);
}
