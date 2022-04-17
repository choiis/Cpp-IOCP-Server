#!groovy

pipeline {
  agent any

  triggers {
    githubPush()
  }
  
  environment {
    AUTHOR = 'Insung Choi'
    VERSION = "${GIT_BRANCH}_${GIT_COMMIT}"
  }

  stages {
    stage('git status') {
      steps {
        echo "GIT_BRANCH=${GIT_BRANCH}"
        echo "GIT_COMMIT=${GIT_COMMIT}"
        echo "VERSION=${VERSION}"
      }
    }

    stage('server build') {
      steps {
		sh 'msbuild /property:Configuration=Release /p:Platform=x86  ./Server/Server.sln'
      }
    }
	
	stage('bot client build') {
      steps {
        sh 'msbuild /property:Configuration=Release /p:Platform=x86  ./BClient/BClient.sln'
      }
    }
	
	stage('client build') {
      steps {
        sh 'msbuild /property:Configuration=Release /p:Platform=x86  ./Client/Client.sln'
      }
    }

  }
}

