#include "host.h"

int main(int argc, char* argv[])
{
      host_header_t *hp;
      host_header_create(&hp);
      host_question_t *qp;
      host_question_create(&qp, argv[1], A);
      char *msg;
      int msg_len = host_query_create(hp, qp, &msg);
      host_query_udp(msg, msg_len);
      return 0;
}