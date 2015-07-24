
//
//	ProEXR AE
// 
//	by Brendan Bolles <brendan@fnordware.com>
//

#ifndef OPENEXR_PREMIERE_DIALOGS_H
#define OPENEXR_PREMIERE_DIALOGS_H

#include <list>

typedef std::list<std::string> ChannelsList;
	
bool	
ProEXR_Channels(
	const ChannelsList	&channels,
	std::string			&red,
	std::string			&green,
	std::string			&blue,
	std::string			&alpha,
	const void			*plugHndl,
	const void			*mwnd);


#endif // OPENEXR_PREMIERE_DIALOGS_H
