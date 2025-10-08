#include <cnet.h>
#include <stdlib.h>
#include <string.h>
#define MAX_CONN 10
#define TIMER(x) EV_TIMER##x
#define TIMEOUTEVENTHANDLER(num) EVENT_HANDLER(timeouts##num){  printf("timeout, connection =" #num ", seq=%i\n", conn[num].ackexpected);writeDataFrame(conn[num].lastframe,num);}

typedef struct {
    char        data[MAX_MESSAGE_SIZE];
}MSG;

typedef struct {
    int seq;                        // -1 if no data, else sequence number
    int ack;                        // -1 if no ACK, else ACK number
    CnetAddr destaddr;
    CnetAddr srcaddr;
    size_t len;
    int checksum;
    MSG msg;
} FRAME;


#define FRAME_HEADER_SIZE  (sizeof(FRAME) - sizeof(MSG))
#define FRAME_SIZE(f)      (FRAME_HEADER_SIZE + f.len)
#define increment(seq)  seq = 1-seq


struct CONN
{
    CnetAddr otheraddr; // address of the other host
    FRAME lastframe;
    CnetTimerID	lasttimer;
    int	nextframetosend;
    int ackexpected;
    int	frameexpected;
    int link;
}conn[MAX_CONN];

struct CONN initializeConnection()
{
    return (struct CONN){.lasttimer = NULLTIMER, .ackexpected=0, .frameexpected=0, .nextframetosend=0, .link=1};
}

int numOpenConn;

void writeDataFrame(FRAME f,int flag) {
    CnetTime	timeout;
    printf(" DATA transmitted, src = %d, dest=%d, seq=%i, ack=%d, msglen=%lu\n", f.srcaddr,f.destaddr,f.seq,f.ack,f.len);
    timeout = FRAME_SIZE(f)*((CnetTime)8000000 / linkinfo[conn[flag].link].bandwidth) + linkinfo[conn[flag].link].propagationdelay;

    switch(flag)
    {
    case 0:conn[flag].lasttimer = CNET_start_timer(TIMER(0), 3 * timeout, 0);break;
    case 1:conn[flag].lasttimer = CNET_start_timer(TIMER(1), 3 * timeout, 0);break;
    case 2:conn[flag].lasttimer = CNET_start_timer(TIMER(2), 3 * timeout, 0);break;
    case 3:conn[flag].lasttimer = CNET_start_timer(TIMER(3), 3 * timeout, 0);break;
    case 4:conn[flag].lasttimer = CNET_start_timer(TIMER(4), 3 * timeout, 0);break;
    case 5:conn[flag].lasttimer = CNET_start_timer(TIMER(5), 3 * timeout, 0);break;
    case 6:conn[flag].lasttimer = CNET_start_timer(TIMER(6), 3 * timeout, 0);break;
    case 7:conn[flag].lasttimer = CNET_start_timer(TIMER(7), 3 * timeout, 0);break;
    case 8:conn[flag].lasttimer = CNET_start_timer(TIMER(8), 3 * timeout, 0);break;
    case 9:conn[flag].lasttimer = CNET_start_timer(TIMER(9), 3 * timeout, 0);break;

    }
    size_t length = FRAME_SIZE(f);
    f.checksum=0;
    f.checksum	= CNET_ccitt((unsigned char *)&f, length);
    printf("CHECKSUM = %d\n",f.checksum);
    conn[flag].lastframe =f;
    CHECK(CNET_write_physical(conn[flag].link, &f, &length));

}

EVENT_HANDLER(application_ready) {
    FRAME f;
    f.len = sizeof(MSG);
    CHECK(CNET_read_application(&(f.destaddr), &(f.msg), &(f.len)));
    CNET_disable_application(ALLNODES);

    int flag =-1;
    for(int i=0;i<numOpenConn;i++) {
        if(conn[i].otheraddr == f.destaddr)
        {
            flag=i;
            break;
        }

    }
     
    if(flag==-1 && numOpenConn<MAX_CONN) // if the connection is not there already form a new one
    {
        conn[numOpenConn]=initializeConnection();
        conn[numOpenConn].otheraddr = f.destaddr;
        flag = numOpenConn;
        numOpenConn++;
    }
    if(flag==-1)
    {
        CNET_enable_application(ALLNODES);
        return;
    }

    f.seq = conn[flag].nextframetosend;
    f.ack = -1;
    f.srcaddr = nodeinfo.address;
    f.checksum=0;
    printf("down from application, seq=%i\n", conn[flag].nextframetosend);
    //transmit_frame(DL_DATA, &lastmsg, lastmsglength, conn[flag].nextframetosend);
    writeDataFrame(f,flag);
    increment(conn[flag].nextframetosend);
}

EVENT_HANDLER(physical_ready) {
     FRAME        frame;
    int          link, arriving_checksum;
    size_t	 len = sizeof(FRAME);

//  RECEIVE THE NEW FRAME
    CHECK(CNET_read_physical(&link, &frame, &len));

//  CALCULATE THE CHECKSUM OF THE ARRIVING FRAME, IGNORE IF INVALID

    arriving_checksum	= frame.checksum;
    frame.checksum  	= 0;
    int x =CNET_ccitt((unsigned char *)&frame, len);
    if(x != arriving_checksum) {
        printf("\t\t\t\tBAD checksum - frame ignored, stored = %d, computed =%d\n",arriving_checksum,x);
        return;           // bad checksum, just ignore frame
    }
    

    int flag = -1;
    for(int i=0;i<numOpenConn;i++) {
        if(conn[i].otheraddr == frame.srcaddr) // as there is only one connection assumed per 2 hosts
        {
            flag=i;
            break;
        }
    }
    if(flag==-1 && numOpenConn<MAX_CONN) { // if the connection is not there already form a new one
        conn[numOpenConn]=initializeConnection();
        conn[numOpenConn].otheraddr = frame.srcaddr;
        flag = numOpenConn;
        numOpenConn++;
    }
    if(flag==-1) {
        return;
    }

    
//  AN ACKNOWLEDGMENT HAS ARRIVED, ENSURE IT'S THE ACKNOWLEDGMENT WE WANT
    
    if(frame.ack != -1 && frame.ack == conn[flag].ackexpected) {
            printf("\t\t\t\t ACK received,src = %d, dest=%d, seq=%i, ack=%d, msglen=%lu\n", frame.srcaddr,frame.destaddr,frame.seq,frame.ack,frame.len);
            CNET_stop_timer(conn[flag].lasttimer);
            increment(conn[flag].ackexpected);
            CNET_enable_application(ALLNODES);
        }
	

//  SOME DATA HAS ARRIVED, ENSURE IT'S THE ONE DATA WE EXPECT
     if(frame.seq != -1) {              //piggybacking supporetd
        printf("\t\t\t\t DATA received,src = %d, dest=%d, seq=%i, ack=%d, msglen=%lu\n", frame.srcaddr,frame.destaddr,frame.seq,frame.ack,frame.len);
        if(frame.seq == conn[flag].frameexpected) {
            printf("up to application\n");
            len = frame.len;
            CHECK(CNET_write_application(&frame.msg, &len));
            increment(conn[flag].frameexpected);
        }
        else
            printf("ignored\n");
        //transmit_frame(DL_ACK, NULL, 0, frame.seq);	// acknowledge the data
        FRAME f ={.seq = -1, .ack = frame.seq, .srcaddr = nodeinfo.address, .destaddr = conn[flag].otheraddr, .len =0, .checksum=0};
        printf(" ACK transmitted,src = %d, dest=%d, seq=%i, ack=%d, msglen=%lu\n", f.srcaddr,f.destaddr,f.seq,f.ack,f.len);
        size_t length	= FRAME_SIZE(f);
        f.checksum	= CNET_ccitt((unsigned char *)&f, length);
        CHECK(CNET_write_physical(conn[flag].link, &f, &length));
	
    }
}

TIMEOUTEVENTHANDLER(0)
TIMEOUTEVENTHANDLER(1)
TIMEOUTEVENTHANDLER(2)
TIMEOUTEVENTHANDLER(3)
TIMEOUTEVENTHANDLER(4)
TIMEOUTEVENTHANDLER(5)
TIMEOUTEVENTHANDLER(6)
TIMEOUTEVENTHANDLER(7)
TIMEOUTEVENTHANDLER(8)
TIMEOUTEVENTHANDLER(9)


static EVENT_HANDLER(showstate) {
    for(int i=0;i<numOpenConn;i++) {
        printf("Connection =%d,\n\tackexpected\t= %i\n\tnextframetosend\t= %i\n\tframeexpected\t= %i\n",i, conn[i].ackexpected, conn[i].nextframetosend, conn[i].frameexpected);
    }
}


EVENT_HANDLER(reboot_node) {
   numOpenConn = 0;

//  INDICATE THE EVENTS OF INTEREST FOR THIS PROTOCOL
    CHECK(CNET_set_handler( EV_APPLICATIONREADY, application_ready, 0));
    CHECK(CNET_set_handler( EV_PHYSICALREADY,    physical_ready, 0));
    CHECK(CNET_set_handler( EV_TIMER0,           timeouts0, 0));
    CHECK(CNET_set_handler( EV_TIMER1,           timeouts1, 0));
    CHECK(CNET_set_handler( EV_TIMER2,           timeouts2, 0));
    CHECK(CNET_set_handler( EV_TIMER3,           timeouts3, 0));
    CHECK(CNET_set_handler( EV_TIMER4,           timeouts4, 0));
    CHECK(CNET_set_handler( EV_TIMER5,           timeouts5, 0));
    CHECK(CNET_set_handler( EV_TIMER6,           timeouts6, 0));
    CHECK(CNET_set_handler( EV_TIMER7,           timeouts7, 0));
    CHECK(CNET_set_handler( EV_TIMER8,           timeouts8, 0));
    CHECK(CNET_set_handler( EV_TIMER9,           timeouts9, 0));

//  BIND A FUNCTION AND A LABEL TO ONE OF THE NODE'S BUTTONS
    CHECK(CNET_set_handler( EV_DEBUG0,           showstate, 0));
    CHECK(CNET_set_debug_string( EV_DEBUG0, "State"));
    
    CNET_enable_application(ALLNODES);
}