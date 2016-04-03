#include "player.h"
#include "player_log.h"


MediaPlayer* MediaPlayer::Create(int use_hard_decode)
{
	MediaPlayer* mp = NULL;
	if(use_hard_decode){
		mp = new MediaPlayerHard();
	}else{
		mp = new MediaPlayerSoft();
	}
	  
	if ( !mp ) {	 
		return NULL;
	}	
	
	return mp;
}

void MediaPlayer::Destroy(MediaPlayer* mp)
{
	if(mp){
		if(mp->using_hard_decode()){
		 	delete static_cast<MediaPlayerHard*>(mp);
		}else{
			delete static_cast<MediaPlayerSoft*>(mp);
		}
	}
}


