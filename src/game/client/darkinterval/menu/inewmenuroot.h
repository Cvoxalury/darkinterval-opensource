class INewMenuRoot
{
public:
	virtual void		Create(vgui::VPANEL parent) = 0;
	virtual void		Destroy(void) = 0;
};

extern INewMenuRoot *newmainmenubase;