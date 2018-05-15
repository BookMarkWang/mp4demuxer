#include <stdlib.h>
#include <stdio.h>
#include "mp4_parse.h"

int main(int argc, char* argv[])
{
	mp4_box_node_t *root = (mp4_box_node_t*)calloc(1, sizeof(mp4_box_node_t));
	get_mp4_box_tree(argv[1], root);
	free(root);
	return 0;
}
