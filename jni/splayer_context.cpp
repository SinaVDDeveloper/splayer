#include "splayer_context.h"


SPlayerEvent *SPlayerEvent::getEvent()
{
    return this;
}

void SPlayerEvent::setEvent(int type, void *data)
{
    type_ = type;
    data_ = data;
}

int SPlayerContext::push(SPlayerEvent event)
{
    return SPLAYER_CONTEXT_ERROR_OK;
}

int SPlayerContext::peep(SPlayerEvent event, int type)
{
    return SPLAYER_CONTEXT_ERROR_OK;
}

int SPlayerContext::wait(SPlayerEvent event)
{
    return SPLAYER_CONTEXT_ERROR_OK;
}
