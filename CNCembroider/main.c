#include <stdio.h>
#include <stdlib.h>
#include "rs232.h"

/* Movement calibration. How many tics per 10mm. */
#define Xscale 500;
#define Yscale 500;

/* Extents, 130mm or 6500 tics per axis. */
//#define Tslp 100;

    int TestMode = 0;   //Test Mode, skips CNC machine check. on com port.

    int i=0,
    cport_nr=0,        /* /dev/ttyS0 (COM1 on windows) */
    bdrate=9600;       /* 9600 baud */

    int firstRun = 1;
    int Tslp = 10;
    int firstStitch = 1;
    int chking = 1;
    int exCheck = 1;
    double Xchk = 0;
    double Ychk = 0;
    int colorNum = 2;
    char mode[]={'8','N','2',0};        //Transmit with two stop bits to let the CNC process.
    unsigned char inputBuf[4095];
    int HeadMode = 0;   //0 = Jump, 1 = Stitch.
    unsigned int StitchNum = 0;
    double Xcmd = 0;
    double Ycmd = 0;
    unsigned char *Farray;
    unsigned char Fname[400];
    long bufsize;
    int Xneg = 1;
    int Yneg = 1;
    unsigned char Xstring[4];
    unsigned char Ystring[4];
    unsigned int XYPoint = 0;
    int fByte = 0;
    int reading = 1;
    FILE *fp;
    int bufPoint;
    unsigned char size = 4095;
    int Copen = 0;
    void getXY(void);
    void XYconvert(void);
    void SendCommand(void);
    void CNCinit(void);
    void nLineSeek(void);
    void deInit(void);


int main()
{
    if (TestMode == 0){
        printf("Looking for CNC device... \n");
        while (Copen == 0){
            i = RS232_OpenComport(cport_nr, bdrate, mode);
            if (i == 1){
                printf("\n");
                printf("Trying next one. \n");
                cport_nr++;
                if (cport_nr >= 21){
                    printf("20 Ports tested and none seemed to open. Press Enter to exit.");
                    getchar();
                    return 0;
                }
            }
            else {
                printf("Success! Testing for CNC device. \n");
                RS232_SendByte(cport_nr, 0x0D);        //CNC device should respond with the letter "a".
                usleep(10000);
                bufPoint = RS232_PollComport(cport_nr, inputBuf, size);
                if (inputBuf[0] == 0x61){
                    printf("Success! We are open!");
                    Copen = 1;
                }
                else {
                    printf("No CNC device found on this port. Testing next port. \n");
                    RS232_CloseComport(cport_nr);
                    cport_nr++;
                    if (cport_nr >= 21){
                        printf("No CNC device found on any ports tested. Press Enter to exit. \n");
                        getchar();
                        return 0;
                    }
                }
            }
        }
        usleep(100000);
        usleep(100000);
        usleep(100000);
        usleep(100000);
    }

    /* Initialize the CNC machine. */
    CNCinit();
    /*******************************/
    printf ("\n");
    printf ("\n Please type the name of the file you want to open. \n \n");
    printf ("#:");
    scanf("%s",Fname);
    printf ("\n Attempting to open file. \n");
    fp=fopen(Fname, "r");
    if (fp != NULL) {
        /* Go to the end of the file. */
        if (fseek(fp, 0L, SEEK_END) == 0) {
            /* Get the size of the file. */
            bufsize = ftell(fp);
            if (bufsize == -1) { printf ("File Open Error. \n"); return 0; }

            /* Allocate our buffer to that size. */
            printf ("Getting file size. \n");
            Farray = malloc(sizeof(unsigned char) * (bufsize + 1));

            /* Go back to the start of the file. */
            if (fseek(fp, 0L, SEEK_SET) != 0) { /* Error */ }

            /* Read the entire file into memory. */
            size_t newLen = fread(Farray, sizeof(unsigned char), bufsize, fp);
            if ( ferror( fp ) != 0 ) {
                fputs("Error reading file", stderr);
            } else {
                Farray[newLen++] = '\0'; /* Just to be safe. */
            }
            printf ("%d \n",bufsize);
            printf ("%d \n", Farray);
        }
        fclose(fp);
    }
    else {
        printf ("\n Failed to open file, or wrong file name. Stopping. \n");
        printf ("\n Press Enter to exit.");
        getchar();
        getchar();
        return 0;
    }
    /*********/
    /*********/
    while (chking){
        if (exCheck){
            printf ("\n Checking pattern range. \n");
        }
        long bufEnd;
        bufEnd = Farray + bufsize;
        printf ("%d \n",bufEnd);
        int cmdCNV = 0;
        printf("\n");
        if (exCheck == 0){
            printf ("System Ready. \n");
            printf ("Press enter when you are ready. \n");
            getchar();
        }
        while (reading == 1 && Farray < bufEnd){
            //printf ("\n Looking for Data... \n");
            if (*Farray==0x23 || *Farray==0x3E){ //Seek to New line if # or >
                while (*Farray != 0x0A && Farray <= bufEnd){  //Seek to new line.
                    Farray++;  //Next byte.
                }
            }
            else if (*Farray == 0x2A){    //Found an "*"
                //printf ("I found an * ! \n");
                    Farray++;  //Next byte.
                if (*Farray == 0x4A){ //JUMP command.
                    if (exCheck == 0){
                        printf ("JUMP. \n");
                    }
                    firstStitch = 1;
                    getXY();
                    //nLineSeek();
                    XYconvert();
                    HeadMode = 0;
                    if (exCheck == 0){
                        SendCommand();
                    }
                }
                else if (*Farray == 0x53){ //STITCH command.
                    if (exCheck == 0){
                        printf ("STITCH. \n");
                        printf ("Stitch Number %d \n",StitchNum);
                    }
                    getXY();
                    //nLineSeek();
                    XYconvert();
                    HeadMode = 1;
                    if (exCheck == 0){
                        SendCommand();
                        StitchNum++;
                    }
                }
                else if (*Farray == 0x43){ //COLOR command.
                    if (exCheck == 0){
                        printf ("COLOR Change. Press enter when ready. \n Color Number %d \n", colorNum);
                        colorNum++;
                        getchar();
                    }
                }
                else if (*Farray == 0x45){  //End command.
                    printf ("\n End \n");
                    break;
                }
                Xchk += Xcmd / 10;
                Ychk += Ycmd / 10;
            }
            else {
                Farray++;  //Next byte.      //Next byte.
            }

        }

        if (reading == 0){
            printf("\n An error has occurred, stopping. Press Enter to Exit. \n");
        }
        else {
            if (exCheck){
                Farray = bufEnd - bufsize;
                /*if (Xchk < -130 || Xchk > 130){
                    printf ("\n Error, X axis is out of range on pattern. \n");
                    chking = 0;
                }
                if (Ychk < -130 || Ychk > 130){
                    printf ("\n Error, Y axis is out of range on pattern. \n");
                    chking = 0;
                }*/
                printf ("\n X range is %f mm. \n",Xchk);
                printf ("\n Y range is %f mm. \n",Ychk);
                if (chking == 0){
                    printf ("\n Press enter to exit. \n");
                    getchar();
                    getchar();
                }
            }
            else {
                /* DeInitialize the CNC for manual control. */
                deInit();
                printf("\n Done. Press Enter to Exit. \n");
                chking = 0;
            }
        }
        if (exCheck == 0){
            getchar();
        }
        exCheck = 0;
    }
    //RS232_cputs(cport_nr, output);

    fclose(fp);
    RS232_CloseComport(cport_nr);       //Close the com port.
    return 0;


}

void nLineSeek(void){
    while (*Farray != 0x0A){  //Seek to new Line.
        Farray++;
    }
}

void deInit(void){
/* Wait for Completion before moving.
    Unless it is a JUMP command. */
    RS232_SendByte(cport_nr, 0x52); // "R"
    //usleep(Tslp);
        RS232_SendByte(cport_nr, 0x6E); // "n"

    /***********************************/
        /* Tool rotation. */
    RS232_SendByte(cport_nr, 0x54); // "T"

         RS232_SendByte(cport_nr, 0x30); // "0"      //Tool on.

    /***********************************/
    /* Set Tool Power Level, Set to 0 if
    it is a JUMP command. */
    RS232_SendByte(cport_nr, 0x50); // "P"

        RS232_SendByte(cport_nr, 0x30); // "0"
        //usleep(Tslp);
        RS232_SendByte(cport_nr, 0x30); // "0"
        //usleep(Tslp);
    /***********************************/
    /* Enter */
    RS232_SendByte(cport_nr, 0x0D);
}

void CNCinit(void){
    RS232_SendByte(cport_nr, 0x0D);
    usleep(Tslp);
     /*Reset the CNC controller and wait. */
    RS232_SendByte(cport_nr, 0x23); // "X" Reset
    usleep(Tslp);
    RS232_SendByte(cport_nr, 0x0D);
    usleep(10000);
    usleep(Tslp);
/* Tool Ready on One Rotation input. */
    RS232_SendByte(cport_nr, 0x43); // "C"
    usleep(Tslp);
    RS232_SendByte(cport_nr, 0x63); // "c"
    usleep(Tslp);
    /***********************************/
    /* Head Needs to move first. */
    RS232_SendByte(cport_nr, 0x4F); // "O"
    usleep(Tslp);
    RS232_SendByte(cport_nr, 0x68); // "h"
    usleep(Tslp);
    /***********************************/
    /* Local echo is OFF. */
    RS232_SendByte(cport_nr, 0x45); // "E"
    usleep(Tslp);
    RS232_SendByte(cport_nr, 0x6E); // "n"
    usleep(Tslp);
    /***********************************/
    /* Head Speed is set to ramp up/down */
    RS232_SendByte(cport_nr, 0x53); // "S"
    usleep(Tslp);
    RS232_SendByte(cport_nr, 0x6E); // "n"
    usleep(Tslp);
    /***********************************/
    /* Head Max Speed */
    RS232_SendByte(cport_nr, 0x4D); // "M"
    usleep(Tslp);
    RS232_SendByte(cport_nr, 0x39); // "9"
    usleep(Tslp);
    RS232_SendByte(cport_nr, 0x39); // "9"
    usleep(Tslp);
    /***********************************/
    /* Enter Key */
    RS232_SendByte(cport_nr, 0x0D);
    usleep(Tslp);
    /************************************/
}


void SendCommand(void){
    /* Wait for Completion before moving.
    Unless it is a JUMP command. */
    RS232_SendByte(cport_nr, 0x52); // "R"
    if (HeadMode == 0){
        RS232_SendByte(cport_nr, 0x6E); // "n"
    }
    else{
        RS232_SendByte(cport_nr, 0x79); // "y"
    }
    /***********************************/
        /* Tool rotation. */
    RS232_SendByte(cport_nr, 0x54); // "T"
    if (HeadMode == 0){
         RS232_SendByte(cport_nr, 0x30); // "0"      //Tool on.
    }
    else{
         RS232_SendByte(cport_nr, 0x31); // "1"      //Tool rotates one time.
    }
    /***********************************/
    /* Set Tool Power Level, Set to 0 if
    it is a JUMP command. */
    RS232_SendByte(cport_nr, 0x50); // "P"
    if (HeadMode == 0){
        RS232_SendByte(cport_nr, 0x30); // "0"
        RS232_SendByte(cport_nr, 0x30); // "0"
    }
    else{
        RS232_SendByte(cport_nr, 0x39); // "9"
        RS232_SendByte(cport_nr, 0x39); // "9"
    }
    /***********************************/
    /***********************************/
    /* X data */
    RS232_SendByte(cport_nr, 0x58); // "X"
    usleep(Tslp);
    if (Xneg < 0){
        RS232_SendByte(cport_nr, 0x2D); //"-"
        usleep(Tslp);
    }
    XYPoint = 0;
    while (XYPoint < 4){
        RS232_SendByte(cport_nr, Xstring[XYPoint]);
        XYPoint++;
    }
    /***********************************/
    /* Y data */
    RS232_SendByte(cport_nr, 0x59); // "Y"
    if (Yneg <= 0){
        RS232_SendByte(cport_nr, 0x2D); //"-"
    }
    XYPoint = 0;
    while (XYPoint < 4){
        RS232_SendByte(cport_nr, Ystring[XYPoint]);
        XYPoint++;
    }
    /************************************/
    /* Enter Key */
    RS232_SendByte(cport_nr, 0x0D);
    /************************************/
    /* Wait for "c" */
    int waiting = 1;
    inputBuf[bufPoint] = 0;
    while (waiting){
        bufPoint = RS232_PollComport(cport_nr, inputBuf, size);
        if (inputBuf[bufPoint] == 0x63){    //Wait for "c"
            inputBuf[bufPoint] = 0;
            waiting = 0;
            printf ("C \n");
        }
        else if (inputBuf[bufPoint] == 0x45){ //Check for "E"
            printf ("\n Syntax Error from CNC machine. \n");
            waiting = 0;
            reading = 0;
        }
    }
}

void XYconvert(void){
    int Xtics = 0;
    int Ytics = 0;
    float Xtemp = 0;
    float Ytemp = 0;
    float Xscl = 0;
    float Yscl = 0;
    Xneg = 1;
    Yneg = 1;
    if (Xcmd < 0){
        Xneg = -1;
        Xcmd *= -1;
    }
    if (Ycmd < 0){
        Yneg = -1;
        Ycmd *= -1;
    }
    Xscl = Xscale;
    Yscl = Yscale;
    Xtemp = Xcmd / 10;
    Ytemp = Ycmd / 10;
    Xtemp *= Xscl;
    Ytemp *= Yscl;
    Xtics = Xtemp;
    Ytics = Ytemp;
    if (Xtics > 9999 || Xtics < -9999){
        Xtics = 0;
        printf ("\n CNC command out of range on X. \n");
        reading = 0;
    }
    if (Ytics > 9999 || Ytics < -9999){
        Ytics = 0;
        printf ("\n CNC command out of range on Y. \n");
        reading = 0;
    }
    //char Xstring[4];
    //char Ystring[4];
    int XYcmp = 1;
    int XYreduc = 1000;
    int XYcnv = 0;
    XYPoint = 0;
    while (XYPoint < 4){
        Xstring[XYPoint] = 0x30;
        XYPoint++;
    }
    XYPoint = 0;
    while (XYPoint < 4){
        while (Xtics >= XYreduc){
            Xtics -= XYreduc;
            Xstring[XYPoint]++;
        }
        XYPoint++;
        XYreduc /= 10;
    }


    XYcmp = 1;
    XYreduc = 1000;
    XYcnv = 0;
    XYPoint = 0;
    while (XYPoint < 4){
        Ystring[XYPoint] = 0x30;
        XYPoint++;
    }
    XYPoint = 0;
    while (XYPoint < 4){
        while (Ytics >= XYreduc){
            Ytics -= XYreduc;
            Ystring[XYPoint]++;
        }
        XYPoint++;
        XYreduc /= 10;
    }
}

void getXY(void){
    int Tneg = 1;
    int Deca = 0;
    float D1[6];
    float T1 = 0;
    float T2 = 0;
    int nLine = 0;

    int h = 0;
                while (h < 3){  //Skip the first three commas.
                    while (*Farray != 0x2C && *Farray != 0x0A){  //Seek to comma.
                        if (*Farray == 0x0A){
                            nLine = 1;
                        }
                        Farray++;  //Next byte.
                    }
                    Farray++;
                    h++;
                }
                if (nLine != 1){
                    /******************************************/
                    T2 = 0;
                    T1 = 0;
                    Tneg = 1;
                    /* Read the number for X, stop at first "." */
                    if (*Farray == 0x2D){     //Is it a - ?
                        Tneg = -1;
                        //printf ("X %d \n", *Farray);
                        Farray++;  //Next byte.
                        //reading = 0;
                    }
                    //printf("X read \n");
                    while (*Farray != 0x2E){      //Stop at "."
                        T1 = *Farray - 0x30;
                        if (T1<0 || T1>9){
                            printf ("Data Error! %d \n", *Farray);
                            reading = 0;
                            break;
                        }
                        T2 *= 10;
                        T2 += T1;
                        Farray++;  //Next byte.
                    }
                    Farray++;
                    Xcmd = T2;
                    Deca = 0;
                    D1[0] = 0;
                    D1[1] = 0;
                    D1[2] = 0;
                    D1[3] = 0;
                    D1[4] = 0;
                    D1[5] = 0;

                    while (Deca <= 4){
                        D1[Deca] = *Farray - 0x30;
                        if (D1[Deca]<0 || D1[Deca]>9){
                            printf ("Data Error for Decimal! %d \n", *Farray);
                            reading = 0;
                        }
                        Deca++;
                        Farray++;
                    }
                    Farray++;
                    D1[0] *= 0.1;
                    D1[1] *= 0.01;
                    D1[2] *= 0.001;
                    D1[3] *= 0.0001;
                    D1[4] *= 0.00001;
                    //D1[5] *= 0.000001;
                    Xcmd += D1[0] + D1[1] + D1[2] + D1[3] + D1[4];
                    Xcmd *= Tneg;
                    if (exCheck == 0){
                        printf ("X %f \n",Xcmd);
                    }
                    /******************************************/
                    while (*Farray != 0x2C){  //Seek to comma.
                        Farray++;  //Next byte.
                    }
                    Farray++;  //Next byte.
                    /******************************************/
                    T2 = 0;
                    T1 = 0;
                    Tneg = 1;
                    /* Read the number for Y, stop at first "." */
                    if (*Farray == 0x2D){     //Is it a - ?
                        Tneg = -1;
                        //printf ("Y %d \n", *Farray);
                        Farray++;  //Next byte.
                        //reading = 0;
                    }
                    //printf("Y read \n");
                    while (*Farray != 0x2E){      //Stop at "."
                        T1 = *Farray - 0x30;
                        if (T1<0 || T1>9){
                            printf ("Data Error for Decimal! %d \n", *Farray);
                            reading = 0;
                            break;
                        }
                        T2 *= 10;
                        T2 += T1;
                        Farray++;  //Next byte.
                    }
                    Farray++;
                    Ycmd = T2;
                    Deca = 0;
                    D1[0] = 0;
                    D1[1] = 0;
                    D1[2] = 0;
                    D1[3] = 0;
                    D1[4] = 0;
                    D1[5] = 0;

                    while (Deca <= 4){
                        D1[Deca] = *Farray - 0x30;
                        if (D1[Deca]<0 || D1[Deca]>9){
                            printf ("Data Error! %d \n", *Farray);
                            reading = 0;
                        }
                        Deca++;
                        Farray++;
                    }
                    Farray++;
                    D1[0] *= 0.1;
                    D1[1] *= 0.01;
                    D1[2] *= 0.001;
                    D1[3] *= 0.0001;
                    D1[4] *= 0.00001;
                    //D1[5] *= 0.000001;
                    Ycmd += D1[0] + D1[1] + D1[2] + D1[3] + D1[4];
                    Ycmd *= Tneg;
                    if (exCheck == 0){
                        printf ("Y %f \n",Ycmd);
                    }
                    /******************************************/
                }
                else {
                    printf ("\n Error. New Line Detected when it shouldn't have! \n");
                    reading = 0;
                }
}
