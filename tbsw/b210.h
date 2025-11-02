int startUsrp();
void stopUsrp();
int changeUsrpSettings(double newFreq, double newRate, double newGain, double newBandwitch);

void requestUSRPStatus();
int usrpStatusAvailable();
char *readUSRPStatus();
void usrpStatusReceived();

