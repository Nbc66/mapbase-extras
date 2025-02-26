#ifndef ATTATCHMENT_UI
#define ATTATCHMENT_UI

class IAttachmentUI
{
public:
	virtual void Create(vgui::VPANEL parent) = 0;
	virtual void Destroy(void) = 0;
};

extern IAttachmentUI* AttachmentUI;

#endif