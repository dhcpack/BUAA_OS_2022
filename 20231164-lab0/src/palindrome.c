#include<stdio.h>
int main()
{
	int n;
	scanf("%d",&n);
	int number = 1;
	int n_copy = n;
	
	while (n_copy!=0){
		n_copy = n_copy / 10;
		number*=10;
	}
	number /= 10;	
	int judge = 1;
		
	while(n!=0){
		if(n/number == n%10){
			n = n%number;
			n = n/10;
			number = number / 100;
		} else {
			judge = 0;
			break;	
		}
	}

	if(judge == 1){
		printf("Y");
	}else{
		printf("N");
	}
	return 0;
}
