package com.splayer.hard;

import java.lang.Thread.UncaughtExceptionHandler;

import android.util.Log;


class SPlayerException implements UncaughtExceptionHandler {
  /**
   * 这里可以做任何针对异常的处理,比如记录日志等等
   */
  public void uncaughtException(Thread a, Throwable e) {
	  Log.e("SPlayerException", "Catch Crash Exception, thread ID=" +  a.getId() + ",Name="+a.getName()
			  +"; Error=" + e.toString());
	  e.printStackTrace();
  }
}