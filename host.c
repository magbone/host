#include "host.h"

void host_header_create(host_header_t **hpp)
{
      *hpp = (host_header_t *)malloc(sizeof(host_header_t));
      if((*hpp) == NULL)
      {
            printf("Error: Init failed\n");
            exit(1);
      }
      (*hpp)->id = (u_int16_t) getpid();
      (*hpp)->flags = 0x0000 /* query */
            + 0x0000 /* standard query */ 
            + 0x0000 /* not truncated */  
            + 0x0100 /* query recurise */ 
            + 0x0000 /* z: reserved(0) */
            + 0x0000 /* record */;
      (*hpp)->qd_count = 0x0001;
      (*hpp)->an_count = 0x0000;
      (*hpp)->ns_count = 0x0000;
      (*hpp)->ar_count = 0x0000;
}

void host_question_create(host_question_t **qpp, char *domain, int type)
{
      *qpp = (host_question_t *)malloc(sizeof(host_question_t));
      if((*qpp) == NULL)
      {
            printf("Error: Init failed\n");
            exit(1);
      }
      host_domain_name_t *np;
      int len = host_domain_name_create(domain, &np);
      (*qpp)->t = np;
      (*qpp)->host_domain_name_size = len;
      (*qpp)->end = 0x00;
      (*qpp)->q_type = 0x0001;
      (*qpp)->q_class = 0x0001;
}

int host_domain_name_create(char *domain, host_domain_name_t **npp)
{

      char *domain_names;
      char *domain_arr[128];
      domain_names = strtok(domain, ".");
      int i = 0;
      while(domain_names)
      {
            domain_arr[i] = domain_names;
            domain_names = strtok(NULL, ".");
            i++;
      }
      
      host_domain_name_t host_domain_name_arr[i];
      for(int j = 0;j < i; j++)
      {
            host_domain_name_arr[j].len = strlen(domain_arr[j]) & 0xff;
            host_domain_name_arr[j].splited_name = (u_int8_t *)domain_arr[j];
      }
      *npp = host_domain_name_arr;
      return i;
}

int host_query_create(host_header_t *hp, host_question_t *qp, char **buffer)
{
      *buffer = (char *)malloc(sizeof(char) * (sizeof(hp) + sizeof(qp)));
      char *buffer_arr = *buffer;
      if (buffer_arr == NULL) {
            printf("Error: Init failed.\n");
            exit(1);
      }
      int index = 0;
      buffer_arr[index++] = ((hp->id) >> 8) & 0xff;
      buffer_arr[index++] = (hp->id) & 0xff;
      buffer_arr[index++] = ((hp->flags) >> 8) & 0xff;
      buffer_arr[index++] = (hp->flags) & 0xff;
      buffer_arr[index++] = ((hp->qd_count) >> 8) & 0xff;
      buffer_arr[index++] = (hp->qd_count) & 0xff;
      buffer_arr[index++] = ((hp->an_count) >> 8) & 0xff;
      buffer_arr[index++] = (hp->an_count) & 0xff;
      buffer_arr[index++] = ((hp->ns_count) >> 8) & 0xff;
      buffer_arr[index++] = (hp->ns_count) & 0xff;
      buffer_arr[index++] = ((hp->ar_count) >> 8) & 0xff;
      buffer_arr[index++] = (hp->ar_count) & 0xff;

      
      for(int i = 0;i < qp->host_domain_name_size;i++)
      {
            buffer_arr[index++] = qp->t[i].len;
            for(int j = 0;j < qp->t[i].len;j++)
            {
                  buffer_arr[index++] = qp->t[i].splited_name[j];
            }
      }
      buffer_arr[index++] = qp->end;
      buffer_arr[index++] = ((qp->q_type) >> 8) & 0xff;
      buffer_arr[index++] = (qp->q_type) & 0xff;
      buffer_arr[index++] = ((qp->q_class) >> 8) & 0xff;
      buffer_arr[index++] = (qp->q_class) & 0xff;
      return index;
}

void host_query_udp(char *buffer, size_t buffer_size)
{
      int sockfd;
      struct sockaddr_in addr;

      memset(&addr, 0, sizeof(addr));
      addr.sin_family = AF_INET;
      addr.sin_addr.s_addr = inet_addr(NAME_SERVER_0);
      addr.sin_port = htons(PORT);

      if((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
      {
            perror("Error[1021]");
            exit(1);
      }

      int addr_len = sizeof(addr);
      char recv_msg[BUFFER_SIZE];
      memset(recv_msg, 0, BUFFER_SIZE);
      char *send_msg = buffer;

      int ret;
      if(sendto(sockfd, send_msg, buffer_size, 0, (struct sockaddr*) &addr,addr_len) < 0)
      {
            perror("Error");
            close(sockfd);     
            exit(1);   
      }

      if((ret = recvfrom(sockfd, recv_msg, BUFFER_SIZE, 0,(struct sockaddr*) &addr,(socklen_t *)&addr_len)) < 0)
      {
            perror("Error");
            if(ret == EWOULDBLOCK || ret == EAGAIN)
            {
                  printf("time out\n");
            }
      }
      

      host_reply_unpack(recv_msg, ret);
      close(sockfd);
}

void host_reply_unpack(char *buffer, int buffer_size)
{
      int index = 0; 
      host_header_t *hp;
      host_header_create(&hp);
      hp->id = ((buffer[0] << 8) & 0xffff) + (buffer[1] & 0x00ff);
      hp->flags = ((buffer[2] << 8) & 0xffff) + (buffer[3] & 0x00ff);
      hp->qd_count = ((buffer[4] << 8) & 0xffff) + (buffer[5] & 0x00ff);
      hp->an_count = ((buffer[6] << 8) & 0xffff) + (buffer[7] & 0x00ff);
      hp->ns_count = ((buffer[8] << 8) & 0xffff) + (buffer[9] & 0x00ff);
      hp->ar_count = ((buffer[10] << 8) & 0xffff) + (buffer[11] & 0x00ff);
      
      index = 12;

      //domain link. the compression.
      host_domain_name_index_t *ip;
      host_domain_name_index_new(&ip);
      //query 
      printf("Name: ");
      char name[256] = {0};
      int name_index = index; 
      while((buffer[index++] & 0xff ) != 0x00)
      {
            int len = buffer[--index] & 0xff;
            index++;
            for(int i = 0; i < len; i++)
            {     
                  char sub_domain[1] = {buffer[index++]};
                  strcat(name, sub_domain);
            }
            strcat(name, ".");
      }
      printf("%s\n",name);
      host_domain_name_index_add(ip, name_index, name);
      index+=4;

      //answers
      for(int i = 0; i < hp->an_count; i++)
      {
            u_int16_t first_octets = ((buffer[index++] << 8) & 0xff00) - 0xc000;
            int an_name = first_octets + (buffer[index++] & 0x00ff);
            first_octets = (buffer[index++] << 8) & 0xff00;
            u_int16_t type =  first_octets + (buffer[index++] & 0x00ff);
            first_octets = (buffer[index++] << 8) & 0xff00;
            u_int16_t class = first_octets + (buffer[index++] & 0x00ff);
            u_int16_t ttl = ((buffer[index] << 24) & 0xff000000)  + ((buffer[index + 1] << 16) & 0x00ff0000)+ ((buffer[index + 2] << 8) & 0x0000ff00)+ (buffer[index + 3] & 0x000000ff);
            index+=4;
            first_octets = (buffer[index++] <<8) & 0xff00;
            u_int16_t length = first_octets + (buffer[index++] & 0x00ff);

            u_int8_t address[length];
            for(int j = 0; j < length; j++)
            {
                 address[j] = (u_int8_t)buffer[index++]; 
            }
            char *rel_name;
            host_domain_name_index_get(ip, an_name, &rel_name);
            printf("\n\n");
            printf("Name: %s\n", rel_name);
            char *type_name;
            GET_TYPE(type, &type_name);
            printf("Type: %s\n", type_name);
            //free(type_name);
            printf("Class: %s\n", GET_CLASS(class));
            printf("TTL: %d\n", ttl);
            printf("Address: %d.%d.%d.%d\n", address[0], address[1], address[2], address[3]);
      }

}

void host_domain_name_index_new(host_domain_name_index_t **ipp)
{
      *ipp = (host_domain_name_index_t *)malloc(sizeof(host_domain_name_index_t));
      if((*ipp) == NULL)
      {
            printf("Error: Init failed.\n");
            exit(1);
      }
      (*ipp)-> next = NULL;
      (*ipp)-> index = 0x3f3f3f3f;
}

void host_domain_name_index_add(host_domain_name_index_t *ip, int index, char *domain)
{
      host_domain_name_index_t *new_p;
      new_p = (host_domain_name_index_t *)malloc(sizeof(host_domain_name_index_t));

      if(new_p == NULL)
      {
            printf("Error: Init failed.\n");
            exit(1);
      }

      host_domain_name_index_t *ip_tmp = ip;
      while(ip_tmp && ip_tmp->next) 
      {
            if(index == ip_tmp->index) return;
            ip_tmp = ip_tmp->next;
      }

      new_p->index = index;
      new_p->domain = domain;
      ip_tmp->next = new_p;
}

void host_domain_name_index_get(host_domain_name_index_t *ip, int index, char **domain)
{
      host_domain_name_index_t* ip_tmp = ip->next;
      
      while(ip_tmp)
      {
            if(ip_tmp->index == index)
            {
                  
                  *domain = ip_tmp->domain;
                  break;
            }
      
            ip_tmp = ip_tmp->next;
      }
}