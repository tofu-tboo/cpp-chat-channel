# 1st indent: 프로젝트 내 대상 파일을 정의

- all: 프로젝트 내 실행 파일을 제외한 모든 파일들에 대해 수행
- ccode: 프로젝트 내 실행 파일을 제외한 모든 c, cpp, h, hpp, tpp 파일들에 대해 수행

# 2nd indent (list): 헤더 내 각각의 define에 대해 수행해야할 작업을 명시

## key

- replace: 해당 키워드를 온전히 대체
- insert: 해당 키워드 다음줄부터 삽입. 단, 중복시키지 않음.

## value

- 단순 문자열
- (3rd indent) {for_sub: {단순 문자열/f-string}}: 모든 파생 클래스에 대해 수행
- (3rd indent) {for_sup: {단순 문자열/f-string}}: 모든 보무 클래스에 대해 수행

# Ex

```
ccode:
	- insert:
		for_sub: "f'friend {0} {1};'"
```

- f-string의 경우 인덱스 구분은 공백이나 탭으로 구분됨.
